#include "ObjectCommApplication.h"

#include "class-a-end-device-lorawan-mac.h"
#include "lora-net-device.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/ObjectCommHeader.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ObjectCommApplication");

NS_OBJECT_ENSURE_REGISTERED(ObjectCommApplication);

TypeId
ObjectCommApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ObjectCommApplication")
                            .SetParent<Application>()
                            .AddConstructor<ObjectCommApplication>()
                            .SetGroupName("lorawan");
    return tid;
}

ObjectCommApplication::ObjectCommApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    m_requestSize = 5;
    m_rng = CreateObject<UniformRandomVariable>();
}

static void callSend(Ptr<ObjectCommApplication> app) {
    app->SendRequest();
}

void ObjectCommApplication::callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);

    NS_LOG_DEBUG("Finished Tx, device: "<<fHdr.GetAddress()<<" success: "<<success<< " total: "<<m_currentReceived<<" first attempt: "<<firstAttempt<<" fcnt: "<<fHdr.GetFCnt());

    // if failure, retry later? need to chose an application delay for that, maybe keeping it low like between 30 sec and 1 min randomly
    if(success == false)
        Simulator::Schedule(Seconds(m_rng->GetInteger(30,60)), &callSend, this);
}

void ObjectCommApplication::callbackReception(std::string context, Ptr<Packet const> packet) {
    // check is ack with data, add to counter and send new packet
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);

    setReceivedTotal(getReceivedTotal()+packetCopy->GetSize());
    NS_LOG_UNCOND("Received ACK with data payload of size " << packetCopy->GetSize() << " Current total data received: " << getReceivedTotal());
    // send a new request if the object is not entirely received
    if (getReceivedTotal() < m_objectSize)
        SendRequest();
}

uint64_t ObjectCommApplication::getReceivedTotal() {
    return m_currentReceived;
}

void ObjectCommApplication::setReceivedTotal(uint64_t amount) {
    m_currentReceived = amount;
}

void ObjectCommApplication::SendRequest()
{
    NS_LOG_FUNCTION(this);

    /** Create and send a new packet
     * structure of the packet: |headers|payload|
     * |payload| = |objID:uint8_t|
    */
    Ptr<Packet> packet = Create<Packet>(0);
    ObjectCommHeader objHeader;
    objHeader.SetObjID(42);
    packet->AddHeader(objHeader);
    NS_LOG_INFO("PACKET SEND WITH OBJEID: "<<(uint64_t) objHeader.GetObjID());
    m_mac->Send(packet);
}

void ObjectCommApplication::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();

    // Make sure we have a MAC layer - taken from oneshot sender
    if (!m_mac)
    {
        // Assumes there's only one device
        Ptr<LoraNetDevice> loraNetDevice = m_node->GetDevice(0)->GetObject<LoraNetDevice>();

        m_mac = loraNetDevice->GetMac();
        m_mac->GetObject<EndDeviceLorawanMac>()->SetFPort(13);
        NS_ASSERT(m_mac);
    }

    m_currentReceived = 0;
    m_mac->TraceConnect("ReceivedPacket", "Received data", MakeCallback(&ObjectCommApplication::callbackReception, this));
    m_mac->TraceConnect("RequiredTransmissions", "Failed or not", MakeCallback(&ObjectCommApplication::callbackCheckEndTx, this));
    // ask for the first fragment
    SendRequest();
}

void ObjectCommApplication::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    // log how much was sent, received etc
}

void ObjectCommApplication::SetObjectSize(uint64_t size) {
    m_objectSize = size;
}

}
}