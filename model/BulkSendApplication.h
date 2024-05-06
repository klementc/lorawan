#ifndef BULK_SEND_APP_H
#define BULK_SEND_APP_H

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
class BulkSendApplication : public Application
{
  public:
    BulkSendApplication();


    static TypeId GetTypeId();

    /**
     * Start the application
     */
    void StartApplication() override;

    /**
     * Stop the application.
     */
    void StopApplication() override;

    void SendRequest();

    uint64_t getSentTotal();
    void setSentTotal(uint64_t);
    void setDataToSend(uint64_t);
    void setRequestSize(uint64_t);

    void callbackReception(std::string context, Ptr<Packet const> packet);
    void callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet);

  private:
    Ptr<LorawanMac> m_mac; //!< The MAC layer of this node.
    uint64_t m_sentTotal; //!< Total size of the object to retrieve
    uint64_t m_requestSize; //!< Size of the payload for the fragment request to send
    Ptr<UniformRandomVariable> m_rng;
    uint64_t m_dataToSend;
};

} // namespace lorawan

} // namespace ns3
#endif /* BULK_SEND_APP_H */
