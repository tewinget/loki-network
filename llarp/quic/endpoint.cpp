#include "endpoint.hpp"
#include "client.hpp"
#include "llarp/net/net_int.hpp"
#include "ngtcp2/ngtcp2.h"
#include "server.hpp"
#include "uvw/async.h"
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/ev/libuv.hpp>

#include <iostream>
#include <random>
#include <variant>

#include <uvw/timer.h>
#include <oxenc/variant.h>
#include <oxenc/endian.h>
#include <oxenc/hex.h>

extern "C"
{
#include <sodium/crypto_generichash.h>
#include <sodium/randombytes.h>
}

// quic packet number decoding code adapted from ngtcp2 internals, as the functions are not
// exposed
namespace {
  const uint8_t *ngtcp2_get_uint32(uint32_t *dest, const uint8_t *p) {
    uint32_t n;
    memcpy(&n, p, sizeof(n));
    *dest = oxenc::big_to_host(n);
    return p + sizeof(n);
  }

  const uint8_t *ngtcp2_get_uint24(uint32_t *dest, const uint8_t *p) {
    uint32_t n = 0;
    memcpy(((uint8_t *)&n) + 1, p, 3);
    *dest = oxenc::big_to_host(n);
    return p + 3;
  }

  const uint8_t *ngtcp2_get_uint16(uint16_t *dest, const uint8_t *p) {
    uint16_t n;
    memcpy(&n, p, sizeof(n));
    *dest = oxenc::big_to_host(n);
    return p + sizeof(n);
  }

  int64_t get_pkt_num(const uint8_t *p, size_t pkt_numlen) {
    uint32_t l;
    uint16_t s;

    switch (pkt_numlen) {
    case 1:
      return *p;
    case 2:
      ngtcp2_get_uint16(&s, p);
      return (int64_t)s;
    case 3:
      ngtcp2_get_uint24(&l, p);
      return (int64_t)l;
    case 4:
      ngtcp2_get_uint32(&l, p);
      return (int64_t)l;
    default:
      return 0;
    }

    return 0;
  }

  int64_t ngtcp2_pkt_adjust_pkt_num(int64_t max_pkt_num, int64_t pkt_num,
                                    size_t n) {
    int64_t expected = max_pkt_num + 1;
    int64_t win = (int64_t)1 << n;
    int64_t hwin = win / 2;
    int64_t mask = win - 1;
    int64_t cand = (expected & ~mask) | pkt_num;

    if (cand <= expected - hwin) {
      assert(cand <= (int64_t)NGTCP2_MAX_VARINT - win);
      return cand + win;
    }
    if (cand > expected + hwin && cand >= win) {
      return cand - win;
    }
    return cand;
  }
}

namespace llarp::quic
{
  static auto logcat = log::Cat("quic");

  Endpoint::Endpoint(EndpointBase& ep) : service_endpoint{ep}
  {
    randombytes_buf(static_secret.data(), static_secret.size());

    // Set up a callback every 250ms to clean up stale sockets, etc.
    expiry_timer = get_loop()->resource<uvw::TimerHandle>();
    expiry_timer->on<uvw::TimerEvent>([this](const auto&, auto&) { check_timeouts(); });
    expiry_timer->start(250ms, 250ms);

    log::debug(logcat, "Created QUIC endpoint");
  }

  Endpoint::~Endpoint()
  {
    if (expiry_timer)
      expiry_timer->close();
  }

  std::shared_ptr<uvw::Loop>
  Endpoint::get_loop()
  {
    auto loop = service_endpoint.Loop()->MaybeGetUVWLoop();
    assert(loop);  // This object should never have been constructed if we aren't using uvw
    return loop;
  }

  // TODO: does the lookup need to be done every single packet?
  //    revisit this during libQUICinet
  // Endpoint::receive_packet(const SockAddr& src, uint8_t ecn, bstring_view data)
  void
  Endpoint::receive_packet(Address remote, uint8_t ecn, const uint8_t second_pktnum_byte, bstring_view data)
  {
    // ngtcp2 wants a local address but we don't necessarily have something so just set it to
    // IPv4 or IPv6 "unspecified" address (0.0.0.0 or ::)
    // SockAddr local = src.isIPv6() ? SockAddr{in6addr_any} : SockAddr{nuint32_t{INADDR_ANY}};

    Packet pkt{
        Path{Address{SockAddr{"::1"sv, huint16_t{0}}, std::nullopt}, remote},
        data,
        ngtcp2_pkt_info{.ecn = ecn}};

    log::trace(logcat, "[{},ecn={}]: received {} bytes", pkt.path, pkt.info.ecn, data.size());

    handle_packet(pkt, second_pktnum_byte);

    log::debug(logcat, "Done handling packet");
  }

  void
  Endpoint::handle_packet(const Packet& p, const uint8_t second_pktnum_byte)
  {
    auto maybe_dcid = handle_packet_init(p);
    if (!maybe_dcid)
    {
      log::debug(logcat, "Handle packet init failed");
      return;
    }
    auto& dcid = *maybe_dcid;

    // See if we have an existing connection already established for it
    log::trace(logcat, "Incoming connection id {}", dcid);
    auto [connptr, alias] = get_conn(dcid);
    if (!connptr)
    {
      if (alias)
      {
        log::debug(logcat, "Incoming packet QUIC CID is an expired alias; dropping");
        return;
      }
      connptr = accept_initial_connection(p);
      if (!connptr)
      {
        log::warning(logcat, "Connection could not be created");
        return;
      }
    }
    if (alias)
      log::debug(logcat, "CID is alias for primary CID {}", connptr->base_cid);
    else
      log::debug(logcat, "CID is primary CID");

    handle_conn_packet(*connptr, p, second_pktnum_byte);
  }

  std::optional<ConnectionID>
  Endpoint::handle_packet_init(const Packet& p)
  {
    ngtcp2_version_cid vi;
    auto rv = ngtcp2_pkt_decode_version_cid(&vi, u8data(p.data), p.data.size(), NGTCP2_MAX_CIDLEN);
    if (rv == NGTCP2_ERR_VERSION_NEGOTIATION)
    {  // Version Negotiation should be sent and otherwise the packet should be ignored
      send_version_negotiation(vi, p.path.remote);
      return std::nullopt;
    }
    if (rv != 0)
    {
      log::warning(logcat, "QUIC packet header decode failed: {}", ngtcp2_strerror(rv));
      return std::nullopt;
    }

    if (vi.dcidlen > ConnectionID::max_size())
    {
      log::warning(logcat, "Internal error: destination ID is longer than should be allowed");
      return std::nullopt;
    }

    return std::make_optional<ConnectionID>(vi.dcid, vi.dcidlen);
  }

  void
  Endpoint::handle_conn_packet(Connection& conn, const Packet& p, const uint8_t second_pktnum_byte)
  {
    if (ngtcp2_conn_is_in_closing_period(conn))
    {
      log::debug(logcat, "Connection is in closing period, dropping");
      close_connection(conn);
      return;
    }
    if (conn.draining)
    {
      log::debug(logcat, "Connection is draining, dropping");
      // "draining" state means we received a connection close and we're keeping the
      // connection alive just to catch (and discard) straggling packets that arrive
      // out of order w.r.t to connection close.
      return;
    }

    /* Because of how quic packet numbers are encoded (and decoded), a quic packet which is too
     * old could be interpreted as having an incorrectly high packet number, e.g. if pktnum 230
     * is delivered and pktnum 400 has already been delivered and ACKed by quic, quic will
     * treat that 230 as 230+256 and send an ACK for packet 486 which doesn't exist, causing
     * a protocol error.  To prevent that, we include the second byte (bits 2^8 through 2^15)
     * of the actual packet number in our lokinet packet and compare that to what ngtcp2 will
     * interpret the packet number as.
     */
    auto pkt = reinterpret_cast<const uint8_t*>(p.data.data());
    auto max_rx_pkt = conn.last_received_pktnum();
    if (max_rx_pkt and (pkt[0] & 0x80) == 0) // only do this on short header packets
    {
      uint8_t pktnum_len = (pkt[0] & 3) + 1;
      int64_t pktnum = get_pkt_num(pkt + 1 + NGTCP2_MAX_CIDLEN, pktnum_len);
      auto adjusted = ngtcp2_pkt_adjust_pkt_num(max_rx_pkt, pktnum, pktnum_len * 8);
      log::debug(logcat, "adjusted: {} = adjust({}, {}, {})", adjusted, max_rx_pkt, pktnum, pktnum_len*8);
      uint8_t adjusted_second_byte = (adjusted >> 8) & 0xFF;
      if (adjusted_second_byte != second_pktnum_byte)
      {
        log::debug(logcat, "Received too-old QUIC packet; pktnum would be parsed incorrectly. Dropping.");
        log::debug(logcat, "ngtcp2 second byte: {}, actual second byte: {}",
            adjusted_second_byte, second_pktnum_byte);
        return;
      }
    }

    if (auto result = read_packet(p, conn); !result)
    {
      log::warning(logcat, "Read packet failed! {}", ngtcp2_strerror(result.error_code));
      log::debug(logcat, "Packet: {}", buffer_printer{p.data});
    }

    // FIXME - reset idle timer?
    log::trace(logcat, "Done with incoming packet");
  }

  io_result
  Endpoint::read_packet(const Packet& p, Connection& conn)
  {
    auto rv =
        ngtcp2_conn_read_pkt(conn, p.path, &p.info, u8data(p.data), p.data.size(), get_timestamp());

    if (rv == 0)
      conn.io_ready();
    else if (rv == NGTCP2_ERR_DRAINING)
    {
      log::debug(logcat, "Draining connection {}", conn.base_cid);
      start_draining(conn);
    }
    else if (rv == NGTCP2_ERR_PROTO)
    {
      log::warning(
          logcat,
          "Immediately closing connection {} due to error {}",
          conn.base_cid,
          ngtcp2_strerror(rv));
      close_connection(conn, rv, "ERR_PROTO"sv);
    }
    else if (rv == NGTCP2_ERR_DROP_CONN)  // drop connection w/o calling
                                          // ngtcp2_conn_write_connection_close()
    {
      log::warning(
          logcat, "Deleting connection {} due to error {}", conn.base_cid, ngtcp2_strerror(rv));
      delete_conn(conn.base_cid);
    }
    else
    {
      log::warning(
          logcat,
          "Immediately closing connection {} due to error {}",
          conn.base_cid,
          ngtcp2_strerror(rv));
      close_connection(conn, rv, ngtcp2_strerror(rv));
    }

    return {rv};
  }

  io_result
  Endpoint::send_packet(const Address& to, bstring_view data, uint8_t ecn, uint8_t second_pktnum_byte)
  {
    assert(service_endpoint.Loop()->inEventLoop());

    size_t header_size = write_packet_header(to.port(), ecn, second_pktnum_byte);
    size_t outgoing_len = header_size + data.size();
    assert(outgoing_len <= buf_.size());
    std::memcpy(&buf_[header_size], data.data(), data.size());
    bstring_view outgoing{buf_.data(), outgoing_len};

    if (service_endpoint.SendToOrQueue(
            *to.endpoint,
            llarp_buffer_t{outgoing.data(), outgoing.size()},
            service::ProtocolType::QUIC))
    {
      log::trace(logcat, "[{}]: sent {}", to, buffer_printer{outgoing});
    }
    else
    {
      log::warning(
          logcat, "Failed to send to quic endpoint {}; was sending {}B", to, outgoing.size());
      return io_result{-1};
    }

    return io_result{0};
  }

  void
  Endpoint::send_version_negotiation(const ngtcp2_version_cid& vi, const Address& source)
  {
    std::array<std::byte, Endpoint::max_pkt_size_v4> buf;
    std::array<uint32_t, NGTCP2_PROTO_VER_MAX - NGTCP2_PROTO_VER_MIN + 2> versions;
    std::iota(versions.begin() + 1, versions.end(), NGTCP2_PROTO_VER_MIN);
    // we're supposed to send some 0x?a?a?a?a version to trigger version negotiation
    versions[0] = 0x1a2a3a4au;

    CSRNG rng{};
    auto nwrote = ngtcp2_pkt_write_version_negotiation(
        u8data(buf),
        buf.size(),
        std::uniform_int_distribution<uint8_t>{0, 255}(rng),
        vi.dcid,
        vi.dcidlen,
        vi.scid,
        vi.scidlen,
        versions.data(),
        versions.size());
    if (nwrote < 0)
      log::warning(
          logcat, "Failed to construct version negotiation packet: {}", ngtcp2_strerror(nwrote));
    if (nwrote <= 0)
      return;

    if (auto rv = send_packet(source, bstring_view{buf.data(), static_cast<size_t>(nwrote)}, 0, 0);
        not rv)
      log::warning(logcat, "Failed to send version negotiation packet");
  }

  void
  Endpoint::close_connection(Connection& conn, int code, std::string_view close_reason)
  {
    log::debug(logcat, "Closing connection {}", conn.base_cid);

    if (!conn || conn.closing || conn.draining)
      return;

    if (code == NGTCP2_ERR_IDLE_CLOSE)
    {
      log::warning(logcat, "Connection {} passed idle expiry timer, closing now", conn.base_cid);
      delete_conn(conn.base_cid);
      return;
    }

    //  "The error not specifically mentioned, including NGTCP2_ERR_HANDSHAKE_TIMEOUT,
    //  should be dealt with by calling ngtcp2_conn_write_connection_close."
    //  https://github.com/ngtcp2/ngtcp2/issues/670#issuecomment-1417300346
    if (code == NGTCP2_ERR_HANDSHAKE_TIMEOUT)
    {
      log::warning(logcat, "Connection {} handshake timed out, closing now", conn.base_cid);
    }

    ngtcp2_ccerr err;
    ngtcp2_ccerr_set_liberr(
        &err,
        code,
        reinterpret_cast<uint8_t*>(const_cast<char*>(close_reason.data())),
        close_reason.size());

    conn.conn_buffer.resize(max_pkt_size_v4);
    ngtcp2_pkt_info pi;

    auto written = ngtcp2_conn_write_connection_close(
        conn,
        &conn.path.path,
        &pi,
        u8data(conn.conn_buffer),
        conn.conn_buffer.size(),
        &err,
        get_timestamp());
    if (written <= 0)
    {
      log::warning(
          logcat,
          "Failed to write connection close packet: {}",
          written < 0 ? ngtcp2_strerror(written) : "unknown error: closing is 0 bytes??");
      log::warning(logcat, "Failed to write packet: removing connection {}", conn.base_cid);
      delete_conn(conn.base_cid);
      return;
    }
    assert(written <= (long)conn.conn_buffer.size());
    conn.conn_buffer.resize(written);
    conn.closing = true;

    assert(conn.closing && !conn.conn_buffer.empty());

    if (auto sent = send_packet(conn.path.remote, conn.conn_buffer, 0,conn.last_sent_pktnum()); not sent)
    {
      log::warning(
          logcat,
          "Failed to send packet: {}; removing connection {}",
          strerror(sent.error_code),
          conn.base_cid);
      delete_conn(conn.base_cid);
      return;
    }
  }

  /// Puts a connection into draining mode (i.e. after getting a connection close).  This will
  /// keep the connection registered for the recommended 3*Probe Timeout, during which we drop
  /// packets that use the connection id and after which we will forget about it.
  void
  Endpoint::start_draining(Connection& conn)
  {
    if (conn.draining)
      return;
    if (conn.on_closing)
    {
      log::trace(logcat, "Calling Connection.on_closing for connection {}", conn.base_cid);
      conn.on_closing(conn);  // only call once
      conn.on_closing = nullptr;
    }
    log::debug(logcat, "Putting {} into draining mode", conn.base_cid);
    conn.draining = true;
    // Recommended draining time is 3*Probe Timeout
    draining.emplace(conn.base_cid, get_time() + ngtcp2_conn_get_pto(conn) * 3 * 1ns);
  }

  void
  Endpoint::check_timeouts()
  {
    auto now = get_time();

    // Destroy any connections that are finished draining
    bool cleanup = false;
    while (!draining.empty() && draining.front().second < now)
    {
      if (auto it = conns.find(draining.front().first); it != conns.end())
      {
        if (std::holds_alternative<primary_conn_ptr>(it->second))
          cleanup = true;
        log::debug(logcat, "Deleting connection {}", it->first);
        conns.erase(it);
      }
      draining.pop();
    }

    if (cleanup)
      clean_alias_conns();
  }

  std::pair<std::shared_ptr<Connection>, bool>
  Endpoint::get_conn(const ConnectionID& cid)
  {
    if (auto it = conns.find(cid); it != conns.end())
    {
      if (auto* wptr = std::get_if<alias_conn_ptr>(&it->second))
        return {wptr->lock(), true};
      return {var::get<primary_conn_ptr>(it->second), false};
    }
    return {nullptr, false};
  }

  bool
  Endpoint::delete_conn(const ConnectionID& cid)
  {
    auto it = conns.find(cid);
    if (it == conns.end())
    {
      log::debug(logcat, "Cannot delete connection {}: cid not found", cid);
      return false;
    }

    bool primary = std::holds_alternative<primary_conn_ptr>(it->second);
    if (primary)
    {
      auto ptr = var::get<primary_conn_ptr>(it->second);
      if (ptr->on_closing)
      {
        log::trace(logcat, "Calling Connection.on_closing for connection {}", cid);
        ptr->on_closing(*ptr);  // only call once
        ptr->on_closing = nullptr;
      }
    }

    log::debug(logcat, "Deleting {} connection {}", primary ? "primary" : "alias", cid);
    conns.erase(it);
    if (primary)
      clean_alias_conns();

    return true;
  }

  void
  Endpoint::clean_alias_conns()
  {
    for (auto it = conns.begin(); it != conns.end();)
    {
      if (auto* conn_wptr = std::get_if<alias_conn_ptr>(&it->second);
          conn_wptr && conn_wptr->expired())
        it = conns.erase(it);
      else
        ++it;
    }
  }

  ConnectionID
  Endpoint::add_connection_id(Connection& conn, size_t cid_length)
  {
    ConnectionID cid;
    for (bool inserted = false; !inserted;)
    {
      cid = ConnectionID::random(cid_length);
      inserted = conns.emplace(cid, conn.weak_from_this()).second;
    }
    log::debug(logcat, "Created cid {} alias for {}", cid, conn.base_cid);
    return cid;
  }

  void
  Endpoint::make_stateless_reset_token(const ConnectionID& cid, unsigned char* dest)
  {
    crypto_generichash_state state;
    crypto_generichash_init(&state, nullptr, 0, NGTCP2_STATELESS_RESET_TOKENLEN);
    crypto_generichash_update(&state, u8data(static_secret), static_secret.size());
    crypto_generichash_update(&state, cid.data, cid.datalen);
    crypto_generichash_final(&state, dest, NGTCP2_STATELESS_RESET_TOKENLEN);
  }

}  // namespace llarp::quic
