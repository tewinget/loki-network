#ifndef LLARP_RC_HPP
#define LLARP_RC_HPP

#include <constants/version.hpp>
#include <crypto/types.hpp>
#include <net/address_info.hpp>
#include <net/exit_info.hpp>
#include <util/aligned.hpp>
#include <util/bencode.hpp>
#include <util/status.hpp>
#include <router_version.hpp>

#include <functional>
#include <nlohmann/json.hpp>
#include <vector>

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

namespace lokimq
{
  class bt_list_consumer;
}  // namespace lokimq

namespace llarp
{
  /// NetID
  struct NetID final : public AlignedBuffer<8>
  {
    static NetID&
    DefaultValue();

    NetID();

    explicit NetID(const byte_t* val);

    explicit NetID(const NetID& other) = default;

    bool
    operator==(const NetID& other) const;

    bool
    operator!=(const NetID& other) const
    {
      return !(*this == other);
    }

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue(ToString());
      return stream;
    }

    std::string
    ToString() const;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;
  };

  inline std::ostream&
  operator<<(std::ostream& out, const NetID& id)
  {
    return id.print(out, -1, -1);
  }

  /// RouterContact
  struct RouterContact
  {
    static constexpr uint64_t CURRENT_RC_VERSION = 1;

    /// for unit tests
    static bool BlockBogons;

    static llarp_time_t Lifetime;
    static llarp_time_t UpdateInterval;
    static llarp_time_t StaleInsertionAge;

    RouterContact()
    {
      Clear();
    }

    struct Hash
    {
      size_t
      operator()(const RouterContact& r) const
      {
        return PubKey::Hash()(r.pubkey);
      }
    };

    // advertised addresses
    std::vector<AddressInfo> addrs;
    // network identifier
    NetID netID;
    // public encryption public key
    llarp::PubKey enckey;
    // public signing public key
    llarp::PubKey pubkey;
    // signature
    llarp::Signature signature;
    /// node nickname, yw kee
    llarp::AlignedBuffer<NICKLEN> nickname;

    llarp_time_t last_updated = 0s;
    uint64_t version = CURRENT_RC_VERSION;
    std::optional<RouterVersion> routerVersion;
    /// should we serialize the exit info?
    const static bool serializeExit = true;

    std::string signed_bt_dict;

    util::StatusObject
    ExtractStatus() const;

    nlohmann::json
    ToJson() const
    {
      return ExtractStatus();
    }

    std::string
    ToString() const
    {
      return ToJson().dump();
    }

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BEncodeSignedSection(llarp_buffer_t* buf) const;

    bool
    operator==(const RouterContact& other) const
    {
      return addrs == other.addrs && enckey == other.enckey && pubkey == other.pubkey
          && signature == other.signature && nickname == other.nickname
          && last_updated == other.last_updated && netID == other.netID;
    }

    bool
    operator<(const RouterContact& other) const
    {
      return pubkey < other.pubkey;
    }

    bool
    operator!=(const RouterContact& other) const
    {
      return !(*this == other);
    }

    void
    Clear();

    bool
    IsExit() const
    {
      return false;
    }

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    bool
    HasNick() const;

    std::string
    Nick() const;

    bool
    IsPublicRouter() const;

    void
    SetNick(std::string_view nick);

    bool
    Verify(llarp_time_t now, bool allowExpired = true) const;

    bool
    Sign(const llarp::SecretKey& secret);

    /// does this RC expire soon? default delta is 1 minute
    bool
    ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 1min) const;

    /// returns true if this RC is expired and should be removed
    bool
    IsExpired(llarp_time_t now) const;

    /// returns time in ms until we expire or 0 if we have expired
    llarp_time_t
    TimeUntilExpires(llarp_time_t now) const;

    /// get the age of this RC in ms
    llarp_time_t
    Age(llarp_time_t now) const;

    bool
    OtherIsNewer(const RouterContact& other) const
    {
      return last_updated < other.last_updated;
    }

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const;

    bool
    Read(const fs::path& fname);

    bool
    Write(const fs::path& fname) const;

    bool
    VerifySignature() const;

   private:
    bool
    DecodeVersion_0(llarp_buffer_t* buf);

    bool
    DecodeVersion_1(lokimq::bt_list_consumer& btlist);
  };

  inline std::ostream&
  operator<<(std::ostream& out, const RouterContact& rc)
  {
    return rc.print(out, -1, -1);
  }

  using RouterLookupHandler = std::function<void(const std::vector<RouterContact>&)>;
}  // namespace llarp

#endif
