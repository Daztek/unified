#pragma once
#include "nwn_api.hpp"
#include "CExoString.hpp"
#include "CVirtualMachineCmdImplementer.hpp"


#ifdef NWN_API_PROLOGUE
NWN_API_PROLOGUE(StackElement)
#endif



typedef uint32_t OBJECT_ID;


struct StackElement
{
    union
    {
        OBJECT_ID   m_nStackObjectID;
        int32_t     m_nStackInt;
        float       m_fStackFloat;
        CExoString  m_sString;
        void*       m_pStackPtr;
    };

    enum Type
    {
        INVALID = 0x00,
        INTEGER = 0x03,
        FLOAT   = 0x04,
        STRING  = 0x05,
        OBJECT  = 0x06,
        ENGST0  = 0x10,
        ENGST1  = 0x11,
        ENGST2  = 0x12,
        ENGST3  = 0x13,
        ENGST4  = 0x14,
        ENGST5  = 0x15,
        ENGST6  = 0x16,
        ENGST7  = 0x17,
        ENGST8  = 0x18,
        ENGST9  = 0x19,
    };

    uint8_t m_nType;

    StackElement() { std::memset(this, 0, sizeof(*this)); }
    ~StackElement() { if (m_nType == Type::STRING) m_sString.Clear(); }

    inline void Init(uint8_t type)
    {
        m_nType = type;
        switch (m_nType)
        {
        case Type::INTEGER:
            m_nStackInt = 0;
            break;
        case Type::FLOAT:
            m_fStackFloat = 0.0f;
            break;
        case Type::OBJECT:
            m_nStackObjectID = NWNXLib::API::Constants::OBJECT_INVALID;
            break;
        case Type::STRING:
            (void)m_sString.Relinquish();
            break;
        case Type::ENGST0:
        case Type::ENGST1:
        case Type::ENGST2:
        case Type::ENGST3:
        case Type::ENGST4:
        case Type::ENGST5:
        case Type::ENGST6:
        case Type::ENGST7:
        case Type::ENGST8:
        case Type::ENGST9:
            m_pStackPtr = nullptr;
            break;
        default:
            return;
        }
    }

    inline void Clear(CVirtualMachineCmdImplementer* vmimpl)
    {
        switch (m_nType)
        {
        case Type::INTEGER:
        case Type::FLOAT:
        case Type::OBJECT:
            break;
        case Type::STRING:
            m_sString.Clear();
            break;
        case Type::ENGST0:
        case Type::ENGST1:
        case Type::ENGST2:
        case Type::ENGST3:
        case Type::ENGST4:
        case Type::ENGST5:
        case Type::ENGST6:
        case Type::ENGST7:
        case Type::ENGST8:
        case Type::ENGST9:
            if (m_pStackPtr)
            {
                vmimpl->DestroyGameDefinedStructure(m_nType - Type::ENGST0, m_pStackPtr);
            }
            break;
        default:
            return;
        }
        memset(this, 0, sizeof(*this));
    }

    inline void CopyFrom(const StackElement& src, CVirtualMachineCmdImplementer* vmimpl)
    {
        m_nType = src.m_nType;
        switch (m_nType)
        {
        case Type::STRING:
            m_sString = src.m_sString;
            break;
        case Type::INTEGER:
            m_nStackInt = src.m_nStackInt;
            break;
        case Type::FLOAT:
            m_fStackFloat = src.m_fStackFloat;
            break;
        case Type::OBJECT:
            m_nStackObjectID = src.m_nStackObjectID;
            break;
        case Type::ENGST0:
        case Type::ENGST1:
        case Type::ENGST2:
        case Type::ENGST3:
        case Type::ENGST4:
        case Type::ENGST5:
        case Type::ENGST6:
        case Type::ENGST7:
        case Type::ENGST8:
        case Type::ENGST9:
            m_pStackPtr = vmimpl->CopyGameDefinedStructure(m_nType - Type::ENGST0, src.m_pStackPtr);
            break;
        default:
            return;
        }
    }

#ifdef NWN_CLASS_EXTENSION_StackElement
    NWN_CLASS_EXTENSION_StackElement
#endif
};


#ifdef NWN_API_EPILOGUE
NWN_API_EPILOGUE(StackElement)
#endif

