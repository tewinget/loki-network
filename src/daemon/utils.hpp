#pragma once

#include "util/logging.hpp"
#include "util/logging/buffer.hpp"

#include <oxenmq/oxenmq.h>

namespace omq = oxenmq;

namespace srouter::controller
{
    static auto logcat = log::Cat("rpc-controller");

    struct rpc_controller;

    struct session_router_instance
    {
        friend struct rpc_controller;

      private:
        static size_t next_id;

      public:
        session_router_instance(omq::ConnectionID c) : ID{++next_id}, cid{std::move(c)} {}

        const size_t ID;
        omq::ConnectionID cid;
    };

    struct rpc_controller
    {
        rpc_controller();

      private:
        std::shared_ptr<omq::OxenMQ> _omq;
        std::unordered_map<omq::address, session_router_instance> _binds;
        std::map<size_t, omq::address> _indexes;

        void _initiate(omq::address src, std::string remote);
        void _status(omq::address src);
        void _close(omq::address src, std::string remote);
        void _halt(omq::address src);

        bool _omq_connect(const std::vector<std::string>& bind_addrs);

      public:
        bool start(std::vector<std::string>& bind_addrs);

        void list_all() const;

        void refresh();

        void initiate(omq::address src, std::string remote);
        void initiate(size_t idx, std::string remote);

        void status(omq::address src);
        void status(size_t idx);

        void close(omq::address src, std::string remote);
        void close(size_t idx, std::string remote);

        void halt(omq::address src);
        void halt(size_t idx);
    };
}  // namespace srouter::controller
