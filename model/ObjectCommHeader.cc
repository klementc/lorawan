
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

MulticastCommandType ObjectCommHeader::getCommandType() const
{
    return MulticastCommandType::DUMMY_COMMAND_TO_REMOVE;
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

/******************************
 * FragSessionSetupReq
 ***************************** */

FragSessionSetupReq::FragSessionSetupReq(uint8_t fragSession,
                      uint16_t nbFrag,
                      uint8_t fragSize,
                      uint8_t control,
                      uint8_t padding,
                      uint32_t descriptor)
    : m_fragSession(fragSession), m_nbFrag(nbFrag), m_fragSize(fragSize),
    m_control(control), m_padding(padding), m_descriptor(descriptor)
{
    NS_LOG_FUNCTION(this << nbFrag << fragSize << control << padding << descriptor);
}

void
FragSessionSetupReq::Print(std::ostream& os) const
{
    os << "| FragSessionSetupReq" << std::endl;
    os << "| fragSession: " << unsigned(m_fragSession) << std::endl;
    os << "| nbFrag: " << unsigned(m_nbFrag) << std::endl;
    os << "| fragSize: " << unsigned(m_fragSize) << std::endl;
    os << "| control: " << unsigned(m_control) << std::endl;
    os << "| padding: " << unsigned(m_padding) << std::endl;
    os << "| descriptor: " << unsigned(m_descriptor) << std::endl;
}

uint32_t
FragSessionSetupReq::GetSerializedSize() const
{
    NS_LOG_FUNCTION_NOARGS();
    return 10;
}

void
FragSessionSetupReq::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteU8(m_fragSession);
    start.WriteU16(m_nbFrag);
    start.WriteU8(m_fragSize);
    start.WriteU8(m_control);
    start.WriteU8(m_padding);
    start.WriteU32(m_descriptor);
}

uint32_t
FragSessionSetupReq::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();

    m_fragSession = start.ReadU8();
    m_nbFrag = start.ReadU16();
    m_fragSize = start.ReadU8();
    m_control = start.ReadU8();
    m_padding = start.ReadU8();
    m_descriptor = start.ReadU32();

    return GetSerializedSize();
}

MulticastCommandType
FragSessionSetupReq::getCommandType() const
{
    NS_LOG_FUNCTION_NOARGS();
    return MulticastCommandType::FRAG_SESSION_SETUP_REQ;
}


/******************************
 * FragSessionSetupAns
 ****************************** */
FragSessionSetupAns::FragSessionSetupAns()
    : m_statusBitMask(0)
{}

void FragSessionSetupAns::Print(std::ostream& os) const
{
    os << "| FragSessionSetupAns" << std::endl;
    os << "| FragIndex: " << unsigned((m_statusBitMask & 0b11000000) >> 6) << std::endl;
    os << "| RFU: " << unsigned((m_statusBitMask & 0b00110000) >> 4) << std::endl;
    os << "| Wrong Descriptor: " << unsigned((m_statusBitMask & 0b00001000) >> 3) << std::endl;
    os << "| FragSession index not supported: " << unsigned((m_statusBitMask & 0b00000100) >> 2) << std::endl;
    os << "| Not Enough Memory: " << unsigned((m_statusBitMask & 0b00000010) >> 1) << std::endl;
    os << "| Encoding unsupported: " << unsigned((m_statusBitMask & 0b00000001)) << std::endl;
}

uint32_t FragSessionSetupAns::GetSerializedSize() const
{
    NS_LOG_FUNCTION_NOARGS();

    return 1;
}

void
FragSessionSetupAns::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteU8(m_statusBitMask);
}

uint32_t
FragSessionSetupAns::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();

    m_statusBitMask = start.ReadU8();
    return GetSerializedSize();
}

MulticastCommandType
FragSessionSetupAns::getCommandType() const
{
    NS_LOG_FUNCTION_NOARGS();

    return MulticastCommandType::FRAG_SESSION_SETUP_ANS;
}

void
FragSessionSetupAns::setFragIndex(uint8_t fragIndex)
{
    NS_LOG_FUNCTION_NOARGS();
    auto oldbm = m_statusBitMask;
    m_statusBitMask |= (fragIndex & 0b11000000);
    NS_LOG_DEBUG("StatusBitMask "<<oldbm<<" -> "<<m_statusBitMask);
}

/******************************
 * Downlink Fragment
 ****************************** */
DownlinkFragment::DownlinkFragment()
    : m_fragmentSize(0), m_indexN(0)
{}

void
DownlinkFragment::Print(std::ostream& os) const
{
    os << "| DownlinkFragment" << std::endl;
    os << "| FragIndex: " << unsigned((m_indexN & 0b1100000000000000) >> 14) << std::endl;
    os << "| N: "<<unsigned((m_indexN & 0b0011111111111111)) << std::endl;
    os << "| Fragment size: "<<unsigned(m_fragmentSize) << std::endl;
}

uint32_t
DownlinkFragment::GetSerializedSize() const
{
    NS_LOG_FUNCTION_NOARGS();

    return m_fragmentSize+2;
}

void
DownlinkFragment::setFragIndex(uint16_t fragIndex)
{
    NS_LOG_FUNCTION_NOARGS();

    NS_ASSERT(fragIndex<4);
    auto oldind = m_indexN;
    m_indexN = (m_indexN & 0x3FFF) | (fragIndex << 8);
    NS_LOG_DEBUG("indexN: "<<oldind<<" -> "<<m_indexN);
}

void
DownlinkFragment::setFragNumber(uint16_t fragNum)
{
    NS_LOG_FUNCTION_NOARGS();

    auto oldind = m_indexN;
    m_indexN = (m_indexN & 0xC000) | fragNum;
    NS_LOG_DEBUG("indexN: "<<oldind<<" -> "<<m_indexN);
}

void
DownlinkFragment::setFragmentSize(uint32_t size)
{
    NS_LOG_FUNCTION_NOARGS();

    NS_ASSERT(size<256);
    auto olds = m_fragmentSize;
    m_fragmentSize = size;
    NS_LOG_DEBUG("fragmentSize: "<<olds<<" -> "<<m_fragmentSize);
    fragment = std::vector<uint8_t>();
    for(int i=0;i<m_fragmentSize;i++) {
        fragment.push_back(0x00);
    }
}

void
DownlinkFragment::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteU16(m_indexN);
    // zero filled fragment, change later to put real data
    for(int i=0;i<m_fragmentSize;i++) {
        start.WriteU8(fragment[i]);
    }
}

uint32_t
DownlinkFragment::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_ASSERT(m_fragmentSize > 0); // user should define the fragment size with predefined value before calling the function

    m_indexN = start.ReadU16();
    for(int i=0;i<m_fragmentSize;i++) {
        fragment.push_back(start.ReadU8());
    }
    return GetSerializedSize();
}

MulticastCommandType
DownlinkFragment::getCommandType() const
{
    return MulticastCommandType::DATA_FRAGMENT;
}

/******************************
 * McClassCSessionReq
 ****************************** */

McClassCSessionReq::McClassCSessionReq()
    : m_mcGroupIdHeader(0),
      m_sessionTime(0),
      m_sessionTimeOut(10),
      m_dlFrequency(0),
      m_dr(0)
{}

void
McClassCSessionReq::Print(std::ostream& os) const
{
    os << "| McClassCSessionReq" << std::endl;
    os << "| McGroupHeadID: " << unsigned(m_mcGroupIdHeader) << std::endl;
    os << "| Session Time: " << unsigned(m_sessionTime) << std::endl;
    os << "| Session Timeout: " << unsigned(m_sessionTimeOut) << std::endl;
    os << "| DL freq: " << unsigned(m_dlFrequency) << std::endl;
    os << "| DR: " << unsigned(m_dr) << std::endl;
}

uint32_t
McClassCSessionReq::GetSerializedSize() const
{
    return 10;
}

void
McClassCSessionReq::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteU8(m_mcGroupIdHeader);
    start.WriteU32(m_sessionTime);
    start.WriteU8(m_sessionTimeOut);
    // 24 bytes to write
    start.WriteU16((uint16_t)(m_dlFrequency>>8));
    start.WriteU8((uint8_t)(m_dlFrequency & 0x000000FF));

    start.WriteU8(m_dr);
}

uint32_t
McClassCSessionReq::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();

    m_mcGroupIdHeader = start.ReadU8();
    m_sessionTime = start.ReadU32();
    m_sessionTimeOut = start.ReadU8();
    m_dlFrequency = start.ReadU16()<<8;
    m_dlFrequency += start.ReadU8();
    m_dr = start.ReadU8();

    return GetSerializedSize();
}

MulticastCommandType
McClassCSessionReq::getCommandType() const
{
    return MulticastCommandType::MC_CLASS_C_SESSION_REQ;
}

void
McClassCSessionReq::setSessionTime(uint32_t sessionTime)
{
    NS_LOG_FUNCTION_NOARGS();

    auto olds = m_sessionTime;
    m_sessionTime = sessionTime;
    NS_LOG_DEBUG("Session Time: "<<olds<<" -> "<<m_sessionTime);
}

void
McClassCSessionReq::setSessionTimeout(uint8_t sessionTimeOut)
{
    NS_LOG_FUNCTION_NOARGS();

    auto olds = m_sessionTimeOut;
    m_sessionTimeOut = sessionTimeOut;
    NS_LOG_DEBUG("Session Timeout: "<<olds<<" -> "<<m_sessionTimeOut);
}

void
McClassCSessionReq::setFrequency(uint32_t freq)
{
    NS_LOG_FUNCTION_NOARGS();

    auto oldf = m_dlFrequency;
    m_dlFrequency = freq;
    NS_LOG_DEBUG("Frequency: "<<oldf<<" -> "<<m_dlFrequency);
}

void
McClassCSessionReq::setDR(uint8_t dr)
{
    NS_LOG_FUNCTION_NOARGS();

    auto oldr = m_dr;
    m_dr = dr;
    NS_LOG_DEBUG("DR: "<<oldr<<" -> "<<(uint32_t)m_dr);
}

/******************************
 * McClassCSessionAns
 ****************************** */
McClassCSessionAns::McClassCSessionAns()
    : m_statusMcGroupID(0),
      m_timeToStart(0)
{}

void
McClassCSessionAns::Print(std::ostream& os) const
{
    os << "| McClassCSessionAns" << std::endl;
    os << "| McGroupHeadID: " << unsigned(m_statusMcGroupID) << std::endl;
    os << "| TimeToStart: " << m_timeToStart << std::endl;
}

uint32_t
McClassCSessionAns::GetSerializedSize() const
{
    return 4;
}

void
McClassCSessionAns::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteU8(m_statusMcGroupID);
    // 24 bytes to write
    start.WriteU16((uint16_t)(m_timeToStart>>8));
    start.WriteU8((uint8_t)(m_timeToStart & 0x000000FF));
}

uint32_t
McClassCSessionAns::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();

    m_statusMcGroupID = start.ReadU8();
    m_timeToStart = start.ReadU16()<<8;
    m_timeToStart += start.ReadU8();

    return GetSerializedSize();
}

MulticastCommandType
McClassCSessionAns::getCommandType() const
{
    return MulticastCommandType::MC_CLASS_C_SESSION_ANS;
}

void
McClassCSessionAns::setTimeToStart(uint32_t timeToStart)
{
    NS_LOG_FUNCTION_NOARGS();
    m_timeToStart = timeToStart;
}

}
}