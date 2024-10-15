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
    void StartApplication() override;
    void StopApplication() override;

    void SetObjectID(uint8_t);

    void SetFPort(uint8_t newPort);
    void SetMinDelayReTx(double delay);

    void callbackReception(std::string context, Ptr<Packet const> packet);
    void callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet);

    /**
     * First request to send towards the NS,
     * to notify of the interest of the device in receiving an update of the data with id m_objectID
     */
    void SendMulticastInitRequest();

    // initialization of the multicast with specification's commands
    void ProcessClassCSessionReq(Ptr<Packet const> packet);
    void ProcessFragSessionSetupReq(Ptr<Packet const> packet);

    // transmission of fragments
    void SendClassCSetupRequest();
    void ProcessMulticastFragRecReq(Ptr<Packet const> packet);

    // If the transmission failed, because of a timeout
    void ReceptionTimeout();

    void PrintLastUpdateTime();
    std::string PrintFragmentMap();

    void SetMCR(double cr);

  private:
    Ptr<ClassAOpenWindowEndDeviceLorawanMac> m_mac; //!< The MAC layer of this node, has to provide open windows
    uint64_t m_objectSize; //!< Total size of the object to retrieve
    uint64_t m_currentReceived;
    uint8_t m_objectID; //!< id of the object the client wants to retrieve
    Ptr<UniformRandomVariable> m_rng;
    double m_min_delay_retransmission {50};
    bool multicastStarted{false};
    double m_frequency;
    uint8_t m_dr;
    EventId m_noMoreFragmentsRx;
    std::vector<bool> m_fragmentMap;
    ns3::Time m_lastUpdate;
    /**
     * Stores the number of fragments necessary for decoding the entire object with the redundancy mechanism
     * We can stop the open window after recieving m_nbFragsToFinish out of size(m_fragmentMap)
     **/
    uint64_t m_nbFragsToFinish;
    // variables related to the fragments and the coding ratio
    uint64_t m_nbFrags;
    uint64_t m_fragSize;
    double m_CR;
    uint32_t m_timeout;
};

} // namespace lorawan

} // namespace ns3
#endif /* OBJECT_COMM_APP_MULTICAST_H */
