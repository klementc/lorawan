
#include "ObjectCommHeader.h"

namespace ns3
{
namespace lorawan
{
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
ObjectCommHeader::SetObjID(uint8_t id) {
  m_objID = id;
}

uint8_t
ObjectCommHeader::GetObjID() {
  return m_objID;
}

void
ObjectCommHeader::Print(std::ostream& os) const
{
    os << "Object comm header size:" << GetSerializedSize() << " object ID: " << m_objID;
}

uint32_t
ObjectCommHeader::GetSerializedSize() const
{
    return 1;
}

void
ObjectCommHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;

    i.WriteU8(m_objID);
}

uint32_t
ObjectCommHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_objID = i.ReadU8();
    return GetSerializedSize();
}

}
}