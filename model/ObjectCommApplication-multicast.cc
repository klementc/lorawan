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

static LorawanMacHeader getMacHdr(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);

    return mHdr;
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    ObjectCommHeader oHdr;
    packetCopy->RemoveHeader(oHdr);
}

static LoraFrameHeader getFrameHdr(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    return fHdr;
}

static ObjectCommHeader getObjHdr(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    ObjectCommHeader oHdr;
    packetCopy->RemoveHeader(oHdr);
    return oHdr;
}

static Ptr<Packet> getObjPayload(Ptr<Packet const> packet) {
        ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    ObjectCommHeader oHdr;
    packetCopy->RemoveHeader(oHdr);
    return packetCopy;
}

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

void ObjectCommApplicationMulticast::SetMinDelayReTx(double delay) {
    NS_LOG_FUNCTION(delay);
    m_min_delay_retransmission = delay;
}

void ObjectCommApplicationMulticast::callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet) {
    LoraFrameHeader fHdr = getFrameHdr(packet);

    NS_LOG_INFO("Finished Tx, device: "<<fHdr.GetAddress()<<" success: "<<success<< " total: "<<m_currentReceived<<" first attempt: "<<firstAttempt<<" fcnt: "<<fHdr.GetFCnt());

    // if failure, retry later
    if(success == false) {
        // if single fragment failed, resend it
        if (fHdr.GetFPort() == ObjectCommHeader::FPORT_SINGLE_FRAG) {
            Simulator::Schedule(Seconds(m_rng->GetInteger(100, 500)), &ObjectCommApplicationMulticast::AskFragments, this);
        } else {
            Simulator::Schedule(Seconds(m_rng->GetInteger(m_min_delay_retransmission, m_min_delay_retransmission+100)), &ObjectCommApplicationMulticast::SendMulticastInitRequest, this);
        }
    }
}

void ObjectCommApplicationMulticast::ProcessMulticastFragRecReq(Ptr<Packet const> packet)
{
    Ptr<Packet> packetCopy = getObjPayload(packet);
    ObjectCommHeader oHdr = getObjHdr(packet);

    NS_LOG_INFO("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)oHdr.GetType()<<" multicastStarted: "<<multicastStarted<<" Fragment: "<< oHdr.GetFragmentNumber());
    m_currentReceived += packetCopy->GetSize();
    m_fragmentMap[oHdr.GetFragmentNumber()] = true;
    // open window for future reception or finish
    if (m_currentReceived<m_objectSize) {
        m_mac->openFreeReceiveWindow(m_frequency, m_dr);
    } else {
        NS_LOG_DEBUG("Received " << m_currentReceived << "/" << m_objectSize << " bytes: stopping now");
        m_mac->closeFreeReceiveWindow();
        multicastStarted = false; // reset for possible future tx or to avoid processing useless packets
    }
    NS_LOG_DEBUG(PrintFragmentMap());
    // cancel previous timeout and schedule new one to stop if not receiving any other fragment
    m_noMoreFragmentsRx.Cancel();
    m_noMoreFragmentsRx = Simulator::Schedule(Seconds(1000), &ObjectCommApplicationMulticast::AskFragments, this);

}

void ObjectCommApplicationMulticast::ProcessSingleFragRecReq(Ptr<Packet const> packet)
{
    Ptr<Packet> packetCopy = getObjPayload(packet);
    ObjectCommHeader oHdr = getObjHdr(packet);

    if (m_currentReceived == 0) return; // CHECK WE DIDNT RECEIVE SOMETHING WE SHOULDNT
    NS_LOG_INFO("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)oHdr.GetType()<<" multicastStarted: "<<multicastStarted<<" Fragment: "<< oHdr.GetFragmentNumber());
    m_currentReceived += packetCopy->GetSize();
    m_fragmentMap[oHdr.GetFragmentNumber()] = true;
    NS_LOG_DEBUG(PrintFragmentMap());
    if (m_currentReceived < m_objectSize) {
        Simulator::Schedule(Seconds(m_rng->GetInteger(50, 100)), &ObjectCommApplicationMulticast::AskFragments, this);
    } else {
        NS_LOG_DEBUG("Received " << m_currentReceived << "/" << m_objectSize << " bytes: stopping now");
        multicastStarted = false; // reset for possible future tx or to avoid processing useless packets
    }
}

void ObjectCommApplicationMulticast::ProcessClassCSessionReq(Ptr<Packet const> packet)
{
    Ptr<Packet> packetCopy = getObjPayload(packet);
    ObjectCommHeader oHdr = getObjHdr(packet);

    NS_LOG_INFO("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)oHdr.GetType()<<" multicastStarted: "<<multicastStarted);
    // Initialise Rx parameters and schedule the open window wakeup
    m_frequency = ObjectCommHeader::GetFrequencyFromIndex(oHdr.GetFreq());
    m_dr = oHdr.GetDR();
    NS_LOG_DEBUG("ACK type 1 bytes: "<< m_currentReceived << "/" << m_objectSize<< " WINDOW time:" <<Seconds(oHdr.GetDelay()*10).GetSeconds()<<" Nb fragments: "<<oHdr.GetFragmentNumber());
    NS_LOG_DEBUG("WINDOW time:" <<Seconds(oHdr.GetDelay()*10).GetSeconds());
    multicastStarted = true;
    m_fragmentMap = std::vector<bool>(oHdr.GetFragmentNumber(), false);
    Simulator::Schedule(Seconds((uint64_t)oHdr.GetDelay()*10),
        &ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow, m_mac, m_frequency, m_dr);
}

void ObjectCommApplicationMulticast::callbackReception(std::string context, Ptr<Packet const> packet) {
    ns3::lorawan::LoraFrameHeader fHdr = getFrameHdr(packet);

    // Setup an open window upon CLASSC_SESS request acknowledgement
    if (fHdr.GetFPort()==ObjectCommHeader::FPORT_CLASSC_SESS && ! multicastStarted)
        ProcessClassCSessionReq(packet);
    // Receive a fragment upon MULTICAST downlink receptions, count size of received data
    else if (fHdr.GetFPort()==ObjectCommHeader::FPORT_MULTICAST && multicastStarted)
        ProcessMulticastFragRecReq(packet);
    // Receive a single fragment downlink, add it to object data
    else if (fHdr.GetFPort()==ObjectCommHeader::FPORT_SINGLE_FRAG && multicastStarted)
        ProcessSingleFragRecReq(packet);
    // if the message is an other type of message not for us, reopen the window, to manage kind of "class C" in the ns-3 module developped for class A
    else {
        NS_LOG_DEBUG("NOT SMART BUT REOPEN WINDOW, THIS MESSAGE WAS PUTTING THE DEVICE TO SLEEP");
        m_mac->openFreeReceiveWindow(m_frequency, m_dr);
    }
}

void ObjectCommApplicationMulticast::AskFragments() {
    // first stop the open rx window
    if(m_mac->checkIsInOpenSlot())
        m_mac->closeFreeReceiveWindow();
    SetFPort(ObjectCommHeader::FPORT_SINGLE_FRAG);

    // find a fragment to request and send the request
    for(size_t i=0; i<m_fragmentMap.size(); i++) {
        if (m_fragmentMap[i] == false) {
            Ptr<Packet> packet = Create<Packet>(0);
            ObjectCommHeader objHeader;
            objHeader.SetObjID(m_objectID);
            objHeader.SetType(3);
            objHeader.SetFragmentNumber(i);
            packet->AddHeader(objHeader);
            NS_LOG_DEBUG("Ask for single fragment "<<(uint64_t) objHeader.GetFragmentNumber());
            m_mac->Send(packet);
            break;
        }
    }
}

std::string ObjectCommApplicationMulticast::PrintFragmentMap() {
    std::ostringstream str;
    int nbRec = 0;
    str << "Fragment Map: [";
    for(size_t i=0; i<m_fragmentMap.size(); i++) {
        if(m_fragmentMap[i]) {
            nbRec++;
            str << "=";
        } else
            str << ".";
    }
    str << "] ("<<nbRec<<"/"<<m_fragmentMap.size()<<")";
    return str.str();
}

void ObjectCommApplicationMulticast::SendMulticastInitRequest()
{
    NS_LOG_FUNCTION(this);
    // Set the FPort according to the standard for this step
    SetFPort(ObjectCommHeader::FPORT_MC_GR_SETUP);
    m_mac->SetMaxNumberOfTransmissions(1);

    Ptr<Packet> packet = Create<Packet>(0);
    ObjectCommHeader objHeader;
    objHeader.SetObjID(m_objectID);
    packet->AddHeader(objHeader);
    NS_LOG_DEBUG("PACKET SEND WITH OBJID: "<<(uint64_t) objHeader.GetObjID());
    m_mac->Send(packet);
}

void ObjectCommApplicationMulticast::SetFPort(uint8_t newPort) {
    NS_ASSERT(m_mac);
    m_mac->SetFPort(newPort);
}

void ObjectCommApplicationMulticast::StartApplication()
{
    if (!m_mac) {
        // Assumes there's only one device
        Ptr<LoraNetDevice> loraNetDevice = m_node->GetDevice(0)->GetObject<LoraNetDevice>();
        m_mac = loraNetDevice->GetMac()->GetObject<ClassAOpenWindowEndDeviceLorawanMac>();
        NS_ASSERT(m_mac);
    }
    NS_LOG_FUNCTION_NOARGS();
    m_currentReceived = 0;
    m_mac->TraceConnect("ReceivedPacket", "Received data", MakeCallback(&ObjectCommApplicationMulticast::callbackReception, this));
    m_mac->TraceConnect("RequiredTransmissions", "Failed or not", MakeCallback(&ObjectCommApplicationMulticast::callbackCheckEndTx, this));
    // ask for the first fragment
    SendMulticastInitRequest();
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