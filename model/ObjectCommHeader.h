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
 * |ObjID:uint8_t|MsgType:uint8_t|MsgFreq:uint8_t|MsgSF:uint8_t|(IF type==1)delay:uint8_t|payload
 * where:
 * - ObjID is the ID of the object to retrieve
 * - MsgType is the type of message:
 *    - 0 = uplink packet
 *    - 1 = downlink ACK
 *    - 2 = downlink Fragment tx
 *    - 3 = single fragment transmission <= special type, not for classic DL multicast
 * - MsgFreq is the frequency to use for future object tx params (0 for uplinks)
 * - MsgSF is the spreading factor to use for future object tx params (0 for uplinks)
 *  - delay is only used for the ack before sending the object to specify a delay after
 * which the first packet of the object will be sent - currently, delay in seconds = delay*10
*/
class ObjectCommHeader : public Header {

  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;


    void SetObjID(uint8_t);

    uint8_t GetObjID();

    void SetType(uint8_t type);

    void SetFreq(uint8_t freq);

    void SetDR(uint8_t dr);

    void SetDelay(uint8_t delay);

    void SetFragmentNumber(uint16_t number);

    void Print(std::ostream& os) const override;

    uint32_t GetSerializedSize() const override;

    void Serialize(Buffer::Iterator start) const override;

    uint32_t Deserialize(Buffer::Iterator start) override;

    uint8_t GetFreq();

    uint8_t GetDR();

    uint8_t GetType();

    uint8_t GetDelay();

    uint16_t GetFragmentNumber();

    /**
     * Get the frequency index, used to set DL freq in the packet without a byte instead of a double
    */
    static uint8_t GetFrequencyIndex(double freq);

    /**
     * Get the frequency fron the frequency index
    */
    static double GetFrequencyFromIndex(uint8_t index);

    /**
     * Used to fill the packet with the object's data, size should be set depending on
     * the size of the data of the object to send and the max size of the lora packet
    */
    //void SetPayloadSize(uint16_t size);

  private:
    uint8_t m_objID{42};
    uint8_t m_type{0};
    uint8_t m_freq{0};
    uint8_t m_dr{0};
    uint8_t m_delay{0};
    uint16_t m_fragmentNumber{0};
};

}
}

#endif /*OBJECT_COMM_HEADER_H*/