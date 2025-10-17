#include "service_manager.hpp"

namespace srouter::sys
{
    NOP_SystemLayerHandler _manager{};
    I_SystemLayerManager* const service_manager = &_manager;
}  // namespace srouter::sys
