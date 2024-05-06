#ifndef OBJECT_COMM_HEADER_H
#define OBJECT_COMM_HEADER_H

#include "ns3/header.h"

namespace ns3
{
namespace lorawan
{

/**
 * Object Communication header
 * Structure for now:
 * |ObjID:uint8_t|
 * where:
 *  - ObjID is the ID of the object to retrieve
*/
class ObjectCommHeader : public Header {

  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;


    void SetObjID(uint8_t);
    uint8_t GetObjID();
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint8_t m_objID{42};
};

}
}

#endif /*OBJECT_COMM_HEADER_H*/