#include "nwnx.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;


NWNX_EXPORT ArgumentStack SetAtPointerInplace(ArgumentStack&& args)
{
    const auto parent = args.extract<JsonEngineStructure>();
    const auto pointer = args.extract<std::string>();
    const auto value = args.extract<JsonEngineStructure>();
    parent.m_shared->m_json[json::json_pointer(pointer)] = value.m_shared->m_json;
    return {};
}

