#ifndef OBJECT_COMM_HEADER_H
#define OBJECT_COMM_HEADER_H

#include "ns3/header.h"
#include "ns3/lorawan-mac-header.h"
#include "ns3/lora-frame-header.h"
#include "ns3/packet.h"

namespace ns3
{
namespace lorawan
{

/**
 * Multicast group list of commands
 * To get info on them, look at documentfragmented_data_block_transport_v1.0.0.pdf from LoRa alliance
 */
enum MulticastCommandType
{
  DUMMY_COMMAND_TO_REMOVE,  // to remove later

  PACKAGE_VERSION_REQ,      // not implemented
  PACKAGE_VERSION_ANS,      // not implemented
  FRAG_STATUS_REQ,          // not implemented
  FRAG_STATUS_ANS,          // not implemented
  FRAG_SESSION_SETUP_REQ,
  FRAG_SESSION_SETUP_ANS,   // not implemented
  FRAG_SESSION_DELETE_REQ,  // not implemented
  FRAG_SESSION_DELETE_ANS,  // not implemented
  DATA_FRAGMENT,
  MC_CLASS_C_SESSION_REQ,
  MC_CLASS_C_SESSION_ANS
};



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

    // FPORTS used at the different steps for the multicast exchange session
    static const uint8_t FPORT_MULTICAST = 200;
    static const uint8_t FPORT_MC_GR_SETUP = 200;
    static const uint8_t FPORT_ED_MC_POLL = 28;
    static const uint8_t FPORT_ED_MC_CLASSC_UP = 29;
    static const uint8_t FPORT_FRAG_SESS_SETUP = 202;
    static const uint8_t FPORT_CLASSC_SESS = 204;

    static const uint8_t FPORT_CLOCK_SYNCH = 203;
    static const uint8_t FPORT_VALIDATION = 205;
    static const uint8_t FPORT_SINGLE_FRAG = 206;

    ObjectCommHeader();
    ObjectCommHeader(uint8_t id);

    void SetObjID(uint8_t);

    virtual MulticastCommandType getCommandType() const;

    uint8_t GetObjID();

    void SetType(uint8_t type);

    void Print(std::ostream& os) const override;

    uint32_t GetSerializedSize() const override;

    void Serialize(Buffer::Iterator start) const override;

    uint32_t Deserialize(Buffer::Iterator start) override;

    uint8_t getCID() const;

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

  protected:
    uint8_t m_cID;
    uint8_t m_objID{42};
};

class FragSessionSetupReq : public ObjectCommHeader {
  /**
   * Fragmentation session setup request command
   *
   * Format of the command:
   * Field        |FragSession| NbFrag| FragSize| Control| Padding| Descriptor|
   * Size (bytes) | 1         | 2     | 1       | 1      | 1      | 4         |
   *
   * Format of FragSession:
   * FragSession Fields  |RFU   |FragIndex |McGroupBitMask|
   * Size (bits)         | 2bits| 2bits    | 4bits        |
   */
  public:
  FragSessionSetupReq(uint8_t fragSession,
                      uint16_t nbFrag,
                      uint8_t fragSize,
                      uint8_t control,
                      uint8_t padding,
                      uint32_t descriptor);

  void Print(std::ostream& os) const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(Buffer::Iterator start) const override;

  uint32_t Deserialize(Buffer::Iterator start) override;

  MulticastCommandType getCommandType() const override;

  uint16_t getNbFrag() const;
  uint8_t getFragSize() const;

  private:
    uint8_t m_fragSession;
    uint16_t m_nbFrag;
    uint8_t m_fragSize;
    uint8_t m_control;
    uint8_t m_padding;
    uint32_t m_descriptor;
};

class FragSessionSetupAns : public ObjectCommHeader {
  /**
   * Fragmentation session setup request command
   *
   * Format of the command:
   * FragSessionSetupAns payload | StatusBitMask |
   * Size (bytes)                | 1             |
   *
   * StatusBitMask detail:
   * Bits       | 7:6     | 5:4 | 3               | 2                              | 1                | 0                   |
    Status bits |FragIndex| RFU | Wrong Descriptor| FragSession index not supported| Not enough Memory| Encoding unsupported|
   */
  public:
  FragSessionSetupAns();

  void Print(std::ostream& os) const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(Buffer::Iterator start) const override;

  uint32_t Deserialize(Buffer::Iterator start) override;

  MulticastCommandType getCommandType() const override;

  void setFragIndex(uint8_t fragIndex);
  private:
    uint8_t m_statusBitMask;
};

class DownlinkFragment :public ObjectCommHeader {
  /**
   * Fragment content packet
   *
   * Format of the command:
   * Size (bytes)  | Index&N | 0:MaxAppPl-3     |
   *               | 2       |  (fragment size) |
   *
   * Index&N format:
   * Index&N Fields | FragIndex | N     |
   * Size (bits)    | 2bits     | 14bits|
   */
  public:
  DownlinkFragment();

  void Print(std::ostream& os) const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(Buffer::Iterator start) const override;

  uint32_t Deserialize(Buffer::Iterator start) override;

  MulticastCommandType getCommandType() const override;
  void setFragIndex(uint16_t fragIndex);
  void setFragNumber(uint16_t N);
  void setFragmentSize(uint32_t size);

  uint16_t getFragNumber();
  uint32_t getFragmentSize();

  private:
    int32_t m_fragmentSize;
    uint16_t m_indexN;
    std::vector<uint8_t> fragment;
};


class McClassCSessionReq :public ObjectCommHeader {
  /**
   * Schedule a multicast session in the future
   * Format of the command:
   * Field        | McGroupIDHeader | Session Time | SessionTimeOut | DLFrequ | DR
   * Size (bytes) | 1               | 4            | 1              | 3       | 1
   *
   * McGroupIDHeader Fields | RFU   | McGroupID
   * Size (bits)            | 6bits | 2bits
   *
   * SessionTimeOut Fields  | RFU   | TimeOut
   * Size (bits)            | 4bits | 4bits
   *
   */
  public:
  McClassCSessionReq();

  void Print(std::ostream& os) const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(Buffer::Iterator start) const override;

  uint32_t Deserialize(Buffer::Iterator start) override;

  MulticastCommandType getCommandType() const override;

  void setSessionTime(uint32_t sessionTime);
  void setSessionTimeout(uint8_t sessionTimeOut);
  void setFrequency(uint32_t freq);
  void setDR(uint8_t dr);
  uint32_t GetFrequency();
  uint8_t GetDR();
  uint32_t GetSessionTime();

  private:
    uint8_t m_mcGroupIdHeader;
    // epoch in seconds
    uint32_t m_sessionTime;
    // on 4 bytes, assert in cpp
    uint8_t m_sessionTimeOut;
    // 3 bytes in the specification, assert in cpp
    // From specification: The actual channel frequency in Hz is 100 x DlFrequ
    uint32_t m_dlFrequency;
    uint8_t m_dr;
};


class McClassCSessionAns :public ObjectCommHeader {
  /**
   * Answer to McClassCSessionReq
   * Format of packet:
   * Field        | Status&McGroupID | (cond)TimeToStart
   * Size (bytes) | 1                | 3
   */
  public:
  McClassCSessionAns();

  void Print(std::ostream& os) const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(Buffer::Iterator start) const override;

  uint32_t Deserialize(Buffer::Iterator start) override;

  MulticastCommandType getCommandType() const override;

  void setTimeToStart(uint32_t timeToStart);

  private:
    // for now, we dont care about these fields
    uint8_t m_statusMcGroupID;
    // used for the server to check the ED is synchronized (in our simulations we assume they are)
    // warning: 3 bytes long field only
    uint32_t m_timeToStart;
};

}
}

#endif /*OBJECT_COMM_HEADER_H*/