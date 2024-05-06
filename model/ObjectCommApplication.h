#ifndef OBJECT_COMM_APP_H
#define OBJECT_COMM_APP_H

#include "lorawan-mac.h"

#include "ns3/application.h"
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
class ObjectCommApplication : public Application
{
  public:
    ObjectCommApplication();


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

    uint64_t getReceivedTotal();
    void setReceivedTotal(uint64_t);

    void callbackReception(std::string context, Ptr<Packet const> packet);
    void callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet);

  private:
    Ptr<LorawanMac> m_mac; //!< The MAC layer of this node.
    uint64_t m_objectSize; //!< Total size of the object to retrieve
    uint64_t m_requestSize; //!< Size of the payload for the fragment request to send
    uint64_t m_currentReceived;
    uint8_t m_objectID; //!< id of the object the client wants to retrieve
    Ptr<UniformRandomVariable> m_rng;

};

} // namespace lorawan

} // namespace ns3
#endif /* OBJECT_COMM_APP */
