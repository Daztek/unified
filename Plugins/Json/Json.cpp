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

NWNX_EXPORT ArgumentStack JsonObjectContainsKey(ArgumentStack&& args)
{
    const auto object = args.extract<JsonEngineStructure>();
    const auto key = args.extract<std::string>();
    return object.m_shared->m_json.is_object() && object.m_shared->m_json.contains(key);
}
