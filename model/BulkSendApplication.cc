#include "BulkSendApplication.h"

#include "class-a-end-device-lorawan-mac.h"
#include "lora-net-device.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/string.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("BulkSendApplication");

NS_OBJECT_ENSURE_REGISTERED(BulkSendApplication);

TypeId
BulkSendApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BulkSendApplication")
                            .SetParent<Application>()
                            .AddConstructor<BulkSendApplication>()
                            .SetGroupName("lorawan");
    return tid;
}

BulkSendApplication::BulkSendApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    m_requestSize = 5;
    m_sentTotal = 0;
    m_dataToSend = 0;
    m_rng = CreateObject<UniformRandomVariable>();
}

static void callSend(Ptr<BulkSendApplication> app) {
    app->SendRequest();
}

void BulkSendApplication::callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);

    NS_LOG_DEBUG("Finished Tx, device: "<<fHdr.GetAddress()<<" success: "<<success<< " total: "<<m_sentTotal<<" first attempt: "<<firstAttempt<<" fcnt: "<<fHdr.GetFCnt());

    // if failure, retry later? need to chose an application delay for that, maybe keeping it low like between 30 sec and 1 min randomly
    if(success == false)
        Simulator::Schedule(Seconds(m_rng->GetInteger(30,60)), &callSend, this);
}

void BulkSendApplication::callbackReception(std::string context, Ptr<Packet const> packet) {
    // check is ack with data, add to counter and send new packet
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);

    setSentTotal(getSentTotal()+m_requestSize);
    NS_LOG_UNCOND("Received ACK with data payload of size " << packetCopy->GetSize() << " Current total data tx: " << getSentTotal());
    // send a new request if the object is not entirely received
    if (getSentTotal() < m_dataToSend)
        SendRequest();
}
void BulkSendApplication::setDataToSend(uint64_t s) {
    m_dataToSend = s;
}
uint64_t BulkSendApplication::getSentTotal() {
    return m_sentTotal;
}

void BulkSendApplication::setSentTotal(uint64_t amount) {
    m_sentTotal = amount;
}

void BulkSendApplication::setRequestSize(uint64_t s) {
    m_requestSize = s;
}
void BulkSendApplication::SendRequest()
{
    NS_LOG_FUNCTION(this);

    // Create and send a new packet
    Ptr<Packet> packet = Create<Packet>(m_requestSize);
    m_mac->Send(packet);
}
void BulkSendApplication::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();

    // Make sure we have a MAC layer - taken from oneshot sender
    if (!m_mac)
    {
        // Assumes there's only one device
        Ptr<LoraNetDevice> loraNetDevice = m_node->GetDevice(0)->GetObject<LoraNetDevice>();

        m_mac = loraNetDevice->GetMac();
        NS_ASSERT(m_mac);
    }

    m_sentTotal = 0;
    m_mac->TraceConnect("ReceivedPacket", "Received data", MakeCallback(&BulkSendApplication::callbackReception, this));
    m_mac->TraceConnect("RequiredTransmissions", "Failed or not", MakeCallback(&BulkSendApplication::callbackCheckEndTx, this));
    // ask for the first fragment
    SendRequest();
}

void BulkSendApplication::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    // log how much was sent, received etc
}
}
}