#ifndef OBJECT_COMM_APP_MULTICAST_H
#define OBJECT_COMM_APP_MULTICAST_H

#include "lorawan-mac.h"

#include "ns3/application.h"
#include "ns3/class-a-openwindow-end-device-lorawan-mac.h"
#include "ns3/attribute.h"
#include "ns3/nstime.h"

namespace ns3
{
namespace lorawan
{

/**
 * \ingroup lorawan
 *
 * Packet sender application to send a single packet
 */
class ObjectCommApplicationMulticast : public Application
{
  public:
    ObjectCommApplicationMulticast();


    static TypeId GetTypeId();

    /**
     * Start the application
     */
    void StartApplication() override;

    /**
     * Stop the application.
     */
    void StopApplication() override;

    void SetObjectSize(uint64_t);

    void SetObjectID(uint8_t);

    void SendRequest();

    void SetMinDelayReTx(double delay);

    uint64_t getReceivedTotal();
    void setReceivedTotal(uint64_t);

    void callbackReception(std::string context, Ptr<Packet const> packet);
    void callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet);

    void AskFragments();
    void PrintFragmentMap();

  private:
    Ptr<ClassAOpenWindowEndDeviceLorawanMac> m_mac; //!< The MAC layer of this node, has to provide open windows
    uint64_t m_objectSize; //!< Total size of the object to retrieve
    uint64_t m_currentReceived;
    uint8_t m_objectID; //!< id of the object the client wants to retrieve
    Ptr<UniformRandomVariable> m_rng;
    double m_min_delay_retransmission {50};
    bool gotAck{false};
    double m_frequency;
    uint8_t m_dr;
    EventId m_noMoreFragmentsRx;
    std::vector<bool> m_fragmentMap;

};

} // namespace lorawan

} // namespace ns3
#endif /* OBJECT_COMM_APP_MULTICAST_H */
