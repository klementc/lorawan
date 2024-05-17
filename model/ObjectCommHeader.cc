
#include "ObjectCommHeader.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ObjectCommHeader");

NS_OBJECT_ENSURE_REGISTERED(ObjectCommHeader);

TypeId
ObjectCommHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ObjectCommHeader")
                            .SetParent<Header>()
                            .SetGroupName("lorawan")
                            .AddConstructor<ObjectCommHeader>();
    return tid;
}

TypeId
ObjectCommHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}


void
ObjectCommHeader::SetObjID(uint8_t id)
{
    NS_LOG_FUNCTION(id);
    m_objID = id;
}

uint8_t
ObjectCommHeader::GetObjID()
{
    NS_LOG_FUNCTION_NOARGS();
    return m_objID;
}

void
ObjectCommHeader::SetType(uint8_t type)
{
    NS_LOG_FUNCTION(type);
    m_type = type;
}

void
ObjectCommHeader::SetFreq(uint8_t freq)
{
    NS_LOG_FUNCTION(freq);
    m_freq = freq;
}

void
ObjectCommHeader::SetDR(uint8_t dr)
{
    NS_LOG_FUNCTION(dr);
    m_dr = dr;
}

void
ObjectCommHeader::SetFragmentNumber(uint16_t number)
{
    NS_LOG_FUNCTION(number);
    m_fragmentNumber = number;
}

void
ObjectCommHeader::Print(std::ostream& os) const
{
    os << "Object comm header size:" << GetSerializedSize() << " object ID: " << (uint64_t)m_objID <<
        " msg type: " << (uint64_t)m_type << " freq: " << (uint64_t)m_freq << " DR: " << (uint64_t)m_dr <<
        " delay: " << (uint64_t)m_delay << " Fragment #: "<< (uint64_t)m_fragmentNumber;
}

uint32_t
ObjectCommHeader::GetSerializedSize() const
{
    // account for the additional delay byte
    if (m_type == 0) return 2; // UL request
    if (m_type == 1) return 6; // DL request ack
    if (m_type == 2 || m_type == 3) return 4; // DL or UL fragment part or request
    return 0;
}

void
ObjectCommHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;

    i.WriteU8(m_objID);
    i.WriteU8(m_type);

    if (m_type == 1) {
        i.WriteU8((m_freq<<4)+m_dr); // 4 left bits = freq, 4 right bits = dr
        i.WriteU8(m_delay);
    }
    if (m_type==1 || m_type == 2 || m_type == 3) {
        i.WriteU16(m_fragmentNumber);
    }
}

uint32_t
ObjectCommHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_objID = i.ReadU8();
    m_type = i.ReadU8();

    if (m_type == 1) {
        uint8_t txParams = i.ReadU8();
        m_freq = (txParams>>4) & 0x0F; // 4 leftmost bits
        m_dr = txParams & 0x0F; // 4 rightmost bits
        m_delay = i.ReadU8();
    }
    if (m_type==1 || m_type == 2 || m_type == 3) {
        m_fragmentNumber = i.ReadU16();
    }

    return GetSerializedSize();
}

uint8_t
ObjectCommHeader::GetFrequencyIndex(double freq)
{
    int freqI = 0;
    if (freq == 868.1) freqI = 1;
    if (freq == 868.3) freqI = 2;
    if (freq == 868.5) freqI = 3;
    if (freq == 869.525) freqI = 4; // could maybe be used for machines far away?

    return freqI;
}

double
ObjectCommHeader::GetFrequencyFromIndex(uint8_t index)
{
    double freq = 0;
    if (index == 1) freq = 868.1;
    if (index == 2) freq = 868.3;
    if (index == 3) freq = 868.5;
    if (index == 4) freq = 869.525; // could maybe be used for machines far away?

    return freq;
}

uint8_t
ObjectCommHeader::GetFreq()
{
    return m_freq;
}

uint8_t
ObjectCommHeader::GetDR()
{
    return m_dr;
}

uint8_t
ObjectCommHeader::GetType()
{
    return m_type;
}

void
ObjectCommHeader::SetDelay(uint8_t delay)
{
    m_delay = delay;
}

uint8_t
ObjectCommHeader::GetDelay()
{
    return m_delay;
}

uint16_t
ObjectCommHeader::GetFragmentNumber()
{
    return m_fragmentNumber;
}

}
}