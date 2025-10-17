#include <session_router.h>

#include <signal.h>

#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

bool _run{true};

using Session_Router_ptr = std::shared_ptr<session_router_context>;

[[nodiscard]] auto
MakeSessionRouter(const std::vector<char>& bootstrap)
{
  auto ctx = std::shared_ptr<session_router_context>(session_router_context_new(), session_router_context_free);
  if (auto err = session_router_add_bootstrap_rc(bootstrap.data(), bootstrap.size(), ctx.get()))
    throw std::runtime_error{strerror(err)};
  if (session_router_context_start(ctx.get()))
    throw std::runtime_error{"could not start context"};
  return ctx;
}

void
WaitForReady(const Session_Router_ptr& ctx)
{
  while (_run and session_router_wait_for_ready(1000, ctx.get()))
  {
    std::cout << "waiting for context..." << std::endl;
  }
}

class Flow
{
  session_router_udp_flowinfo const _info;
  session_router_context* const _ctx;

 public:
  explicit Flow(const session_router_udp_flowinfo* info, session_router_context* ctx) : _info{*info}, _ctx{ctx}
  {}

  session_router_context*
  Context() const
  {
    return _ctx;
  }

  std::string
  String() const
  {
    std::stringstream ss;
    ss << std::string{_info.remote_host} << ":" << std::to_string(_info.remote_port)
       << " on socket " << _info.socket_id;
    return ss.str();
  }
};

struct ConnectJob
{
  session_router_udp_flowinfo remote;
  session_router_context* ctx;
};

void
CreateOutboundFlow(void* user, void** flowdata, int* timeout)
{
  auto* job = static_cast<ConnectJob*>(user);
  Flow* flow = new Flow{&job->remote, job->ctx};
  *flowdata = flow;
  *timeout = 30;
  std::cout << "made outbound flow: " << flow->String() << std::endl;
  ;
}

int
ProcessNewInboundFlow(void* user, const session_router_udp_flowinfo* remote, void** flowdata, int* timeout)
{
  auto* ctx = static_cast<session_router_context*>(user);
  Flow* flow = new Flow{remote, ctx};
  std::cout << "new udp flow: " << flow->String() << std::endl;
  *flowdata = flow;
  *timeout = 30;

  return 0;
}

void
DeleteFlow(const session_router_udp_flowinfo* remote, void* flowdata)
{
  auto* flow = static_cast<Flow*>(flowdata);
  std::cout << "udp flow from " << flow->String() << " timed out" << std::endl;
  delete flow;
}

void
HandleUDPPacket(const session_router_udp_flowinfo* remote, const char* pkt, size_t len, void* flowdata)
{
  auto* flow = static_cast<Flow*>(flowdata);
  std::cout << "we got " << len << " bytes of udp from " << flow->String() << std::endl;
}

void
BounceUDPPacket(const session_router_udp_flowinfo* remote, const char* pkt, size_t len, void* flowdata)
{
  auto* flow = static_cast<Flow*>(flowdata);
  std::cout << "bounce " << len << " bytes of udp from " << flow->String() << std::endl;
  if (auto err = session_router_udp_flow_send(remote, pkt, len, flow->Context()))
  {
    std::cout << "bounce failed: " << strerror(err) << std::endl;
  }
}

Session_Router_ptr sender, recip;

void
signal_handler(int)
{
  _run = false;
}

int
main(int argc, char* argv[])
{
  if (argc == 1)
  {
    std::cout << "usage: " << argv[0] << " bootstrap.signed" << std::endl;
    return 1;
  }

  /*
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  */

  std::vector<char> bootstrap;

  // load bootstrap.signed
  {
    std::ifstream inf{argv[1], std::ifstream::ate | std::ifstream::binary};
    size_t len = inf.tellg();
    inf.seekg(0);
    bootstrap.resize(len);
    inf.read(bootstrap.data(), bootstrap.size());
  }

  if (auto* loglevel = getenv("SROUTER_LOG"))
    session_router_log_level(loglevel);
  else
    session_router_log_level("none");

  std::cout << "starting up" << std::endl;

  recip = MakeSessionRouter(bootstrap);
  WaitForReady(recip);

  session_router_udp_bind_result recipBindResult{};

  const auto port = 10000;

  if (auto err = session_router_udp_bind(
          port,
          ProcessNewInboundFlow,
          BounceUDPPacket,
          DeleteFlow,
          recip.get(),
          &recipBindResult,
          recip.get()))
  {
    std::cout << "failed to bind recip udp socket " << strerror(err) << std::endl;
    return 0;
  }

  std::cout << "bound recip udp" << std::endl;

  sender = MakeSessionRouter(bootstrap);
  WaitForReady(sender);

  std::string recipaddr{session_router_address(recip.get())};

  std::cout << "recip ready at " << recipaddr << std::endl;

  session_router_udp_bind_result senderBindResult{};

  if (auto err = session_router_udp_bind(
          port,
          ProcessNewInboundFlow,
          HandleUDPPacket,
          DeleteFlow,
          sender.get(),
          &senderBindResult,
          sender.get()))
  {
    std::cout << "failed to bind sender udp socket " << strerror(err) << std::endl;
    return 0;
  }

  ConnectJob connect{};
  connect.remote.socket_id = senderBindResult.socket_id;
  connect.remote.remote_port = port;
  std::copy_n(recipaddr.c_str(), recipaddr.size(), connect.remote.remote_host);
  connect.ctx = sender.get();

  std::cout << "bound sender udp" << std::endl;

  do
  {
    std::cout << "try establish to " << connect.remote.remote_host << std::endl;
    if (auto err =
            session_router_udp_establish(CreateOutboundFlow, &connect, &connect.remote, sender.get()))
    {
      std::cout << "failed to establish to recip: " << strerror(err) << std::endl;
      usleep(100000);
    }
    else
      break;
  } while (true);
  std::cout << "sender established" << std::endl;

  const std::string buf{"libsessionrouter"};

  const std::string senderAddr{session_router_address(sender.get())};

  do
  {
    std::cout << senderAddr << " send to remote: " << buf << std::endl;
    if (auto err = session_router_udp_flow_send(&connect.remote, buf.data(), buf.size(), sender.get()))
    {
      std::cout << "send failed: " << strerror(err) << std::endl;
    }
    usleep(100000);
  } while (_run);
  return 0;
}
