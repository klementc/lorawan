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

static LoraFrameHeader getFrameHdr(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    return fHdr;
}

static Ptr<Packet> getLoRaPayload(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    return packetCopy;
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
    : m_frequency(-1), m_dr(0), m_lastUpdate(0)
{
    NS_LOG_FUNCTION_NOARGS();
    m_rng = CreateObject<UniformRandomVariable>();
    PrintLastUpdateTime();
}

void ObjectCommApplicationMulticast::SetMinDelayReTx(double delay) {
    NS_LOG_FUNCTION(delay);
    m_min_delay_retransmission = delay;
}

void ObjectCommApplicationMulticast::callbackCheckEndTx(std::string context, uint8_t reqTx, bool success, Time firstAttempt, Ptr<Packet> packet) {
    LoraFrameHeader fHdr = getFrameHdr(packet);

    NS_LOG_INFO("Finished Tx, device: "<<fHdr.GetAddress()<<" success: "<<success<< " total: "<<m_currentReceived<<" first attempt: "<<firstAttempt<<" fcnt: "<<fHdr.GetFCnt()<<" FPort: "<<unsigned(fHdr.GetFPort()));

    // if failure, retry later
    if(success == false) {
        // failed getting class C info? try again TODO: consider the timeout of the frag session
        if (fHdr.GetFPort() == ObjectCommHeader::FPORT_ED_MC_CLASSC_UP && multicastStarted != false) {
            NS_LOG_INFO("FAILED GETTING CLASS C FEEDBACK");
            Simulator::Schedule(Seconds(m_rng->GetInteger(100, 500)), &ObjectCommApplicationMulticast::SendClassCSetupRequest, this);
        }
        // avoid restarting if it is already started, but send just the CLASSC request until timeout
        else if (multicastStarted==false) {
            Simulator::Schedule(Seconds(m_rng->GetInteger(m_min_delay_retransmission, m_min_delay_retransmission+100)), &ObjectCommApplicationMulticast::SendMulticastInitRequest, this);
        }
    }
}

void ObjectCommApplicationMulticast::ReceptionTimeout()
{
    NS_LOG_INFO("TODO");
    NS_LOG_INFO("Failed to retrieve object "<<m_objectID<<" Received "<<m_currentReceived/m_fragSize<<" fragments when we required "<<m_nbFragsToFinish<<". Cancel the reception now");
    m_mac->closeFreeReceiveWindow();
    multicastStarted = false;
}

void ObjectCommApplicationMulticast::ProcessMulticastFragRecReq(Ptr<Packet const> packet)
{
    Ptr<Packet> packetCopy = getLoRaPayload(packet);
    DownlinkFragment dlHdr;
    packetCopy->RemoveHeader(dlHdr);

    // we received a fragment, so we cancel the timeout
    m_noMoreFragmentsRx.Cancel();

    m_currentReceived += dlHdr.getFragmentSize();
    m_fragmentMap[dlHdr.getFragNumber()] = true;
    // open window for future reception or finish
    if (m_currentReceived < m_nbFragsToFinish*m_fragSize) {
        m_mac->openFreeReceiveWindow(m_frequency, m_dr);
        // we set the timeout again since we are waiting for more fragments
        m_noMoreFragmentsRx = Simulator::Schedule(Seconds(m_timeout), &ObjectCommApplicationMulticast::ReceptionTimeout, this);
    } else {
        m_lastUpdate = Simulator::Now();
        NS_LOG_DEBUG("Received " << m_currentReceived << "/" << m_objectSize << " bytes: stopping now, we have enough fragments to reconstruct the original data. Nb_frag_rec / Nb_frag_total: "<<m_nbFragsToFinish<<"/"<<m_nbFrags<<" with CR= "<<m_CR);
        m_mac->closeFreeReceiveWindow();
        multicastStarted = false; // reset for possible future tx or to avoid processing useless packets
    }
    NS_LOG_DEBUG(PrintFragmentMap());
    // cancel previous timeout and schedule new one to stop if not receiving any other fragment
}

void ObjectCommApplicationMulticast::ProcessClassCSessionReq(Ptr<Packet const> packet)
{
    McClassCSessionReq cHdr;
    getLoRaPayload(packet)->RemoveHeader(cHdr);

    // Initialise Rx parameters and schedule the open window wakeup
    multicastStarted = true;
    m_frequency = cHdr.GetFrequency()/1e4;
    m_dr = cHdr.GetDR();
    m_timeout = std::pow(2,cHdr.GetSessionTimeout());
    NS_LOG_INFO("Received class C info: "<<m_frequency<<" "<< (uint32_t) m_dr <<" "<<cHdr.GetSessionTime()<<" scheduled at time "<<Seconds(cHdr.GetSessionTime()));

    //////////// NOW SCHEDULE THE OPEN WINDOW AT THE RIGHT TIME
    if (Seconds(cHdr.GetSessionTime())<Simulator::Now()) {
        NS_LOG_INFO("Failed reception because got answer too late, try again later...");
    } else {
        Simulator::Schedule(Seconds(cHdr.GetSessionTime())-Simulator::Now(),
            &ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow, m_mac, m_frequency, m_dr);
    }
}

void ObjectCommApplicationMulticast::PrintLastUpdateTime()
{
    NS_LOG_INFO("Last update: "<<m_lastUpdate.GetSeconds()<<" s.");
    Simulator::Schedule(Seconds(3600), &ObjectCommApplicationMulticast::PrintLastUpdateTime, this);
}

void ObjectCommApplicationMulticast::SendClassCSetupRequest()
{
    NS_LOG_INFO("Now send class C session request");

    // just send an empty packet to receive the class C schedule
    SetFPort(ObjectCommHeader::FPORT_ED_MC_CLASSC_UP);
    Ptr<Packet> dummypacket = Create<Packet>(0);

    m_mac->Send(dummypacket);
}

void ObjectCommApplicationMulticast::ProcessFragSessionSetupReq(Ptr<Packet const> packet)
{
    // Initiate a communication to get the MulticastSessionReq with FPORT_ED_MC_CLASSC_UP
    NS_LOG_INFO("Received PACKET FRAGSETUP from "<<m_mac->GetDeviceAddress());

    auto pck = getLoRaPayload(packet);
    FragSessionSetupReq req(0,0,0,0,0,0);
    pck->RemoveHeader(req);

    // Initialise variables for reception
    m_currentReceived = 0;
    m_nbFrags = req.getNbFrag();
    m_fragSize = req.getFragSize();
    m_objectSize = m_nbFrags * m_fragSize;
    m_fragmentMap = std::vector<bool>(req.getNbFrag(), false);
    /**
     * m_nbFragsToFinish is the average number of blocks to recover the original object
     * the value is based on Gallager's  results showing the average number of fragments
     * to receive for full receptino. We use it to avoid simulating the whole recovery process
     */
    NS_LOG_INFO("Using CR= "<<m_CR);
    m_nbFragsToFinish = (m_CR * (double)m_nbFrags) + 2;
    NS_LOG_DEBUG("ACK Frag setup: Object of " << m_objectSize<< "bytes, Nb fragments: "<<m_fragmentMap.size()<<" fragment Size: "<< (uint32_t)req.getFragSize());

    Simulator::Schedule(Seconds(20), &ObjectCommApplicationMulticast::SendClassCSetupRequest, this);
}

void ObjectCommApplicationMulticast::callbackReception(std::string context, Ptr<Packet const> packet) {
    ns3::lorawan::LoraFrameHeader fHdr = getFrameHdr(packet);

    // Setup an open window upon CLASSC_SESS request acknowledgement
    if (fHdr.GetFPort()==ObjectCommHeader::FPORT_CLASSC_SESS && ! multicastStarted)
        ProcessClassCSessionReq(packet);
    else if (fHdr.GetFPort()==ObjectCommHeader::FPORT_FRAG_SESS_SETUP && ! multicastStarted)
        ProcessFragSessionSetupReq(packet);
    // Receive a fragment upon MULTICAST downlink receptions, count size of received data
    else if (fHdr.GetFPort()==ObjectCommHeader::FPORT_MULTICAST && multicastStarted)
        ProcessMulticastFragRecReq(packet);
    // Receive a single fragment downlink, add it to object data
    // else if (fHdr.GetFPort()==ObjectCommHeader::FPORT_SINGLE_FRAG && multicastStarted)
    //     ProcessSingleFragRecReq(packet);
    // if the message is an other type of message not for us, reopen the window, to manage kind of "class C" in the ns-3 module developped for class A
    else {
        NS_LOG_DEBUG("NOT SMART BUT REOPEN WINDOW, THIS MESSAGE WAS PUTTING THE DEVICE TO SLEEP");
        m_mac->openFreeReceiveWindow(m_frequency, m_dr);
        return;
    }
    Ptr<Packet> packetCopy = getObjPayload(packet);
    ObjectCommHeader req;
    packetCopy->RemoveHeader(req);

    NS_LOG_INFO("Received ACK on "<< m_mac->GetDeviceAddress()<< " type: "<<(uint64_t)req.getCommandType()<<" multicastStarted: "<<multicastStarted<<" FPort: "<<unsigned(fHdr.GetFPort()));
}

void ObjectCommApplicationMulticast::SetMCR(double cr)
{
    NS_LOG_FUNCTION(cr);
    NS_LOG_INFO("CR is set to "<< cr);
    m_CR = cr;
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
    SetFPort(ObjectCommHeader::FPORT_ED_MC_POLL);
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

}
}