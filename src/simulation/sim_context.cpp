#include "sim_context.hpp"

#include <session/router_context.hpp>

namespace srouter
{
    namespace simulate
    {
        Simulation::Simulation() : m_CryptoManager(new sodium::CryptoLibSodium()) {}

        void Simulation::NodeUp(srouter::Context*) {}

        Node_ptr Simulation::AddNode(const std::string& name)
        {
            auto itr = m_Nodes.find(name);
            if (itr == m_Nodes.end())
            {
                itr = m_Nodes.emplace(name, std::make_shared<srouter::Context>(shared_from_this())).first;
            }
            return itr->second;
        }

        void Simulation::DelNode(const std::string& name) { m_Nodes.erase(name); }
    }  // namespace simulate
}  // namespace srouter
