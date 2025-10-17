#pragma once
#include "crypto/crypto.hpp"
#include "ev/ev.hpp"

namespace srouter
{
    // forward declair
    struct Context;
    using Node_ptr = std::shared_ptr<srouter::Context>;

    namespace simulate
    {
        struct Simulation : public std::enable_shared_from_this<Simulation>
        {
            Simulation();

            srouter::CryptoManager m_CryptoManager;
            // std::shared_ptr<quic::Loop> m_NetLoop;

            std::unordered_map<std::string, Node_ptr> m_Nodes;

            void NodeUp(srouter::Context* node);

            Node_ptr AddNode(const std::string& name);

            void DelNode(const std::string& name);
        };

        using Sim_ptr = std::shared_ptr<Simulation>;
    }  // namespace simulate
}  // namespace srouter
