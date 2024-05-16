#include "ObjectCommApplication-multicast.h"

#include "class-a-openwindow-end-device-lorawan-mac.h"
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

NS_LOG_COMPONENT_DEFINE("ObjectCommApplicationMulticast");

NS_OBJECT_ENSURE_REGISTERED(ObjectCommApplicationMulticast);

TypeId
ObjectCommApplicationMulticast::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ObjectCommApplicationMulticast")
                            .SetParent<Application>()
                            .AddConstructor<ObjectCommApplicationMulticast>()
                            .SetGroupName("lorawan");
    return tid;
}

ObjectCommApplicationMulticast::ObjectCommApplicationMulticast()
    : m_frequency(-1), m_dr(0)
{
    NS_LOG_FUNCTION_NOARGS();
    m_rng = CreateObject<UniformRandomVariable>();
}

static void callSend(Ptr<ObjectCommApplicationMulticast> app) {
    app->SendRequest();
}

void ObjectCommApplicationMulticast::SetMinDelayReTx(double delay) {
    NS_LOG_FUNCTION(delay);
    m_min_delay_retransmission = delay;
}

void ObjectCommApplicationMulticast::callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);

    NS_LOG_DEBUG("Finished Tx, device: "<<fHdr.GetAddress()<<" success: "<<success<< " total: "<<m_currentReceived<<" first attempt: "<<firstAttempt<<" fcnt: "<<fHdr.GetFCnt());

    // if failure, retry later? need to chose an application delay for that, maybe keeping it low like between 30 sec and 1 min randomly
    if(success == false) {
        Simulator::Schedule(Seconds(m_rng->GetInteger(m_min_delay_retransmission, m_min_delay_retransmission+100)), &callSend, this);
    }
}

void ObjectCommApplicationMulticast::callbackReception(std::string context, Ptr<Packet const> packet) {
    // check is ack with data, add to counter and send new packet
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    ObjectCommHeader oHdr;
    packetCopy->RemoveHeader(oHdr);
    std::ostringstream oss;
    oHdr.Print(oss);


    //static double frequency = ObjectCommHeader::GetFrequencyFromIndex(oHdr.GetFreq());
    //uint8_t dr = oHdr.GetDR();
    // if message is ack type 1, schedule opening of the first window
    if (oHdr.GetType() == 1 && ! gotAck) {
        NS_LOG_DEBUG("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)oHdr.GetType()<<" gotACK: "<<gotAck);
        // init reception parameters
        m_frequency = ObjectCommHeader::GetFrequencyFromIndex(oHdr.GetFreq());
        m_dr = oHdr.GetDR();
        NS_LOG_INFO("Received ACK type 1, current bytes: "<< getReceivedTotal() << "/" << m_objectSize<< " WINDOW time:" <<Seconds(oHdr.GetDelay()*10).GetSeconds());
        NS_LOG_INFO("WINDOW time:" <<Seconds(oHdr.GetDelay()*10).GetSeconds());
        gotAck = true;
        Simulator::Schedule(Seconds((uint64_t)oHdr.GetDelay()*10),
            &ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow, m_mac, m_frequency, m_dr);
    } // if message is ack type 2, add datareceived to total and open window for next fragment if necessary
    else if (oHdr.GetType() == 2 && gotAck) {
        NS_LOG_DEBUG("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)oHdr.GetType()<<" gotACK: "<<gotAck);
        setReceivedTotal(getReceivedTotal()+packetCopy->GetSize());
        NS_LOG_INFO("Received " << getReceivedTotal() << "/" << m_objectSize << " bytes: open free window");
        // open window for future reception or finish
        if (getReceivedTotal()<m_objectSize) {
            m_mac->openFreeReceiveWindow(m_frequency, m_dr);
        } else {
            NS_LOG_INFO("Received " << getReceivedTotal()  << "/" << m_objectSize << " bytes: stopping now");
            m_mac->closeFreeReceiveWindow();
            gotAck = false; // reset for possible future tx or to avoid processing useless packets
        }
    } // if the message is an other type of message not for us, reopen the window
    else {
        NS_LOG_INFO("NOT SMART BUT REOPEN WINDOW, THIS MESSAGE WAS PUTTING THE DEVICE TO SLEEP");
        m_mac->openFreeReceiveWindow(m_frequency, m_dr);
    }
}

uint64_t ObjectCommApplicationMulticast::getReceivedTotal() {
    return m_currentReceived;
}

void ObjectCommApplicationMulticast::setReceivedTotal(uint64_t amount) {
    m_currentReceived = amount;
}

void ObjectCommApplicationMulticast::SendRequest()
{
    NS_LOG_FUNCTION(this);
    m_mac->SetMaxNumberOfTransmissions(1);

    Ptr<Packet> packet = Create<Packet>(0);
    ObjectCommHeader objHeader;
    objHeader.SetObjID(42);
    packet->AddHeader(objHeader);
    NS_LOG_INFO("PACKET SEND WITH OBJID: "<<(uint64_t) objHeader.GetObjID());
    m_mac->Send(packet);
}

void ObjectCommApplicationMulticast::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();

    // Make sure we have a MAC layer - taken from oneshot sender
    if (!m_mac)
    {
        // Assumes there's only one device
        Ptr<LoraNetDevice> loraNetDevice = m_node->GetDevice(0)->GetObject<LoraNetDevice>();

        m_mac = loraNetDevice->GetMac()->GetObject<ClassAOpenWindowEndDeviceLorawanMac>();
        m_mac->SetFPort(13);
        NS_ASSERT(m_mac);
    }

    m_currentReceived = 0;
    m_mac->TraceConnect("ReceivedPacket", "Received data", MakeCallback(&ObjectCommApplicationMulticast::callbackReception, this));
    m_mac->TraceConnect("RequiredTransmissions", "Failed or not", MakeCallback(&ObjectCommApplicationMulticast::callbackCheckEndTx, this));
    // ask for the first fragment
    SendRequest();
}

void ObjectCommApplicationMulticast::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    // log how much was sent, received etc
}

void ObjectCommApplicationMulticast::SetObjectSize(uint64_t size) {
    m_objectSize = size;
}

}
}