/*
 * Copyright (c) 2018 University of Padova
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#include "network-controller-components.h"
#include "ns3/ObjectCommHeader.h"

#include "ObjectUtility.h" // temporary, to get the size of the object, use better solution later

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("NetworkControllerComponent");

NS_OBJECT_ENSURE_REGISTERED(NetworkControllerComponent);

static LoraFrameHeader getFrameHdr(Ptr<Packet const> packet) {
    ns3::Ptr<ns3::Packet> packetCopy = packet->Copy();
    ns3::lorawan::LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    ns3::lorawan::LoraFrameHeader fHdr;
    packetCopy->RemoveHeader(fHdr);
    return fHdr;
}

TypeId
NetworkControllerComponent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NetworkControllerComponent").SetParent<Object>().SetGroupName("lorawan");
    return tid;
}

NetworkControllerComponent::NetworkControllerComponent()
{
}

NetworkControllerComponent::~NetworkControllerComponent()
{
}

////////////////////////////////
// ConfirmedMessagesComponent //
////////////////////////////////
TypeId
ConfirmedMessagesComponent::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ConfirmedMessagesComponent")
                            .SetParent<NetworkControllerComponent>()
                            .AddConstructor<ConfirmedMessagesComponent>()
                            .SetGroupName("lorawan");
    return tid;
}

ConfirmedMessagesComponent::ConfirmedMessagesComponent()
{
}

ConfirmedMessagesComponent::~ConfirmedMessagesComponent()
{
}

void
ConfirmedMessagesComponent::OnReceivedPacket(Ptr<const Packet> packet,
                                             Ptr<EndDeviceStatus> status,
                                             Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);

    // Check whether the received packet requires an acknowledgment.
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    Ptr<Packet> myPacket = packet->Copy();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);

    NS_LOG_INFO("Received packet Mac Header: " << mHdr);
    NS_LOG_INFO("Received packet Frame Header: " << fHdr);

    if (fHdr.GetFPort() == ObjectCommHeader::FPORT_ED_MC_POLL){
        ProcessUOTARequest(packet, status, networkStatus);
    }
    else if (fHdr.GetFPort() == ObjectCommHeader::FPORT_ED_MC_CLASSC_UP) {
        CreateClassCMulticastReq(packet, status, networkStatus);
    }
    else if (mHdr.GetMType() == LorawanMacHeader::CONFIRMED_DATA_UP)
    {
        NS_LOG_INFO("Packet requires confirmation");

        // Set up the ACK bit on the reply
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.SetAck(true);
        status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;
        //status->m_reply.payload = Create<Packet>(230);
        status->m_reply.payload = Create<Packet>(0);

        // Note that the acknowledgment procedure dies here: "Acknowledgments
        // are only snt in response to the latest message received and are never
        // retransmitted". We interpret this to mean that only the current
        // reception window can be used, and that the Ack field should be
        // emptied in case transmission cannot be performed in the current
        // window. Because of this, in this component's OnFailedReply method we
        // void the ack bits.
    }
}

bool alreadyScheduled = false;
double freq = 0;
uint8_t dr = 10;
double emitTime = 0;
std::pair<ConfirmedMessagesComponent::ObjectPhase, double> currentState = std::make_pair(ConfirmedMessagesComponent::ObjectPhase::initialize, 0);
std::vector<std::pair<LoraTag,LoraDeviceAddress>> registeredNodes;
LoraDeviceAddress devAddr;
enum txParamsPolicy {ALL_MACHINES, FASTEST_MACHINES, N_PERCENT};
txParamsPolicy selectedPolicy = txParamsPolicy::ALL_MACHINES;

// uses registered nodes and the selected policy to define the parameters for transmission of the model
std::pair<LoraTag, LoraDeviceAddress> ConfirmedMessagesComponent::SelectParamsForBroadcast()
{
    std::pair<LoraTag, LoraDeviceAddress> selectedParams = registeredNodes[0];
    double lowestDR;
    double drtmp;
    switch (selectedPolicy) {
        case txParamsPolicy::ALL_MACHINES:
            // Policy 1: All machines receive the message -> select the highest SF
            lowestDR = 100; // compute lowest DR possible
            for (auto params : registeredNodes) {
                NS_LOG_INFO("Processing params for "<<params.second<<" SF: "<<(uint64_t)params.first.GetSpreadingFactor()<<" Freq: "<<params.first.GetFrequency());
                // compute if this node is better than the previously selected
                drtmp = 0;
                for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==params.first.GetSpreadingFactor()) {drtmp=i;break;}}
                if (drtmp < lowestDR) {
                    lowestDR = drtmp;
                    selectedParams = params;
                }
            }
            break;
        case txParamsPolicy::FASTEST_MACHINES:
            // Policy 2: Send the update only to the closest set of machines -> select the lowest SF
            lowestDR = 0; // compute lowest DR possible
            for (auto params : registeredNodes) {
                NS_LOG_INFO("Processing params for "<<params.second<<" SF: "<<(uint64_t)params.first.GetSpreadingFactor()<<" Freq: "<<params.first.GetFrequency());
                // compute if this node is better than the previously selected
                drtmp = 0;
                for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==params.first.GetSpreadingFactor()) {drtmp=i;break;}}
                if (drtmp > lowestDR) {
                    lowestDR = drtmp;
                    selectedParams = params;
                }
            }
            break;
        case txParamsPolicy::N_PERCENT:
            NS_LOG_ERROR("TODO, NOT IMPLEMENTED YET");
            break;
        default:
            NS_LOG_ERROR("Tx paramaters policy does not exist");
            exit(1);
    }
    selectedParams.first.SetFrequency(869.525);
    return selectedParams;

}

void ConfirmedMessagesComponent::ProcessUOTARequest(Ptr<const Packet> packet,
                                             Ptr<EndDeviceStatus> status,
                                             Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);
    static std::pair<LoraTag, LoraDeviceAddress> dest;
    static bool isDestDefined = false;
    LoraFrameHeader fHdr = getFrameHdr(packet);

    if(currentState.first == ObjectPhase::pool && currentState.second+1000<=(Simulator::Now()).GetSeconds()) {
        // must switch to advertising phase
        currentState = std::make_pair(ObjectPhase::advertize, Simulator::Now().GetSeconds());
        isDestDefined = false;
    } else if (currentState.first == ObjectPhase::initialize){
        currentState = std::make_pair(ObjectPhase::pool, Simulator::Now().GetSeconds());
    }
    if(currentState.first == ObjectPhase::pool) {
        // Pooling phase, do not send any ACK during this phase
        NS_LOG_INFO("POOLING");
        status->m_reply.frameHeader.SetAck(false);
        status->m_reply.needsReply = false;

        // save tx params if it is what we want
        Ptr<Packet> myPacket = packet->Copy();
        LoraTag tagtmp;
        myPacket->RemovePacketTag(tagtmp);
        registeredNodes.push_back(std::make_pair(tagtmp, fHdr.GetAddress()));

    } else if (currentState.first == ObjectPhase::advertize) {
        // First step: compute the Tx parameters to use
        if (! isDestDefined){ // TODO: check this is working correctly
            dest = SelectParamsForBroadcast();
            isDestDefined = true;
        }

        // Acknowledge clients with the broadcast information
        NS_LOG_INFO("ADVERTISING");
        status->m_reply.frameHeader.SetFPort(ObjectCommHeader::FPORT_FRAG_SESS_SETUP);
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.SetAck(true);
        status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;
        auto retPL = Create<Packet>(0);

        if (! alreadyScheduled) {
            //take freq and sf from LoRa tag
            freq = dest.first.GetFrequency();
            dr = 0;
            for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==dest.first.GetSpreadingFactor()) {dr=i;break;}}
            emitTime = (Simulator::Now()+Seconds(3600)).GetSeconds();
        }

        ////////////// PART 1 SEND THE FRAG SESSION REQUEST AND SCHEDULE THE EMISSION
        DownlinkFragment fragUseless; // just to get the size of the header
        auto plSize = 0.;
        for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==dest.first.GetSpreadingFactor()) {plSize = maxPLsize[i]-fragUseless.GetSerializedSize();break;}}
        double nbFragmentsNoRedundancy = std::ceil(OBJECT_SIZE_BYTES/plSize);
        uint32_t nbFragmentsWithRedundancy = std::ceil(nbFragmentsNoRedundancy/CR);
        FragSessionSetupReq fragHdr(0,
                                    nbFragmentsWithRedundancy,
                                    static_cast<uint8_t>(plSize),
                                    0, // TODO
                                    0,
                                    0);

        retPL->AddHeader(fragHdr);
        status->m_reply.payload = retPL;
        std::stringstream str;
        fragHdr.Print(str);
        NS_LOG_DEBUG("Send DownlinkFragment"<<str.str());

        NS_LOG_INFO("Request for object ID: "<< (uint64_t)fragHdr.GetObjID()<< " already sent: "<<alreadyScheduled);

        // Schedule the future emission
        if(! alreadyScheduled) {
            EmitObject(networkStatus->GetReplyForDevice(dest.second, 3), networkStatus);
            alreadyScheduled = true;
        }
    }
}

void
ConfirmedMessagesComponent::CreateClassCMulticastReq(Ptr<const Packet> packet,
                                             Ptr<EndDeviceStatus> status,
                                             Ptr<NetworkStatus> networkStatus)
{

    // PREPARE THE CLASS C SESSION REQUEST
    McClassCSessionReq sHdr;
    sHdr.setDR(dr);
    sHdr.setFrequency(freq*1e4); // freq is in MHz, multiply by 1e6, divide by 100, see specification
    uint32_t sessionTime = static_cast<uint32_t>(emitTime-1);
    // If someone asks for opening the session too late. TODO: setup for starting reception and using redundant fragments for finishing
    if (sessionTime < 0) {
        status->m_reply.frameHeader.SetAck(false);
        status->m_reply.needsReply = false;
    }
    sHdr.setSessionTime(sessionTime);
    sHdr.setSessionTimeout(10); // Client will schedule a timeout after 2**10 seconds to close the window if receiving nothing

    // prepare the ACK
    LoraFrameHeader fHdr = getFrameHdr(packet);

    status->m_reply.frameHeader.SetFPort(ObjectCommHeader::FPORT_CLASSC_SESS);
    status->m_reply.frameHeader.SetAsDownlink();
    status->m_reply.frameHeader.SetAck(true);
    status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
    status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    status->m_reply.needsReply = true;

    auto retPL = Create<Packet>(0);
    retPL->AddHeader(sHdr);
    status->m_reply.payload = retPL;

    NS_LOG_INFO("Send Class C Session Request to "<<fHdr.GetAddress());
}

void ConfirmedMessagesComponent::SwitchToState(ObjectPhase phase){
    currentState = std::make_pair(phase, Simulator::Now().GetSeconds());
    // if we reset the alg, we need to reset alreadySent
    if (phase == ObjectPhase::initialize) alreadyScheduled = false;
}

void ConfirmedMessagesComponent::EmitObject(Ptr<Packet> packetTemplate, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_INFO("CALL TO EMIT");
    // switch to sending since we start sending now

    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    DownlinkFragment oHdr;
    LoraTag tag;

    Ptr<Packet> pcktdata = packetTemplate->Copy();
    pcktdata->RemoveHeader(mHdr);
    pcktdata->RemoveHeader(fHdr);
    pcktdata->RemovePacketTag(tag);
    auto gwToUse = networkStatus->GetBestGatewayForDevice(fHdr.GetAddress(), 1);
    fHdr.SetAddress(LoraDeviceAddress(0,0));

    NS_LOG_INFO("USING GW "<<gwToUse << " for dev "<< fHdr.GetAddress());
    if (gwToUse == Address()) {
        NS_LOG_INFO("Schedule emit after 1 more second");
        Simulator::Schedule(Seconds(1),
            &ConfirmedMessagesComponent::EmitObject,
            this,
            packetTemplate, networkStatus);
        return;
    }

    // compute the size of the payload we are able to send given the tx parameters
    // rp002-1-0-4-regional-parameters.pdf page 48 to find the max payload sizes to use
    fHdr.SetFPort(ObjectCommHeader::FPORT_MULTICAST);

    int payloadSize = maxPLsize[dr]-oHdr.GetSerializedSize();

    Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0)),
        &ConfirmedMessagesComponent::SwitchToState, this, ObjectPhase::send);

    double nbFragmentsNoRedundancy = std::ceil(OBJECT_SIZE_BYTES/payloadSize);
    uint32_t nbFragmentsWithRedundancy = std::ceil(nbFragmentsNoRedundancy/CR);
    NS_LOG_INFO("nb fragments no red: "<<nbFragmentsNoRedundancy<<" with CR="<<CR<<", nb fragments ="<<nbFragmentsWithRedundancy);
    for(uint32_t nbSent=0;nbSent<nbFragmentsWithRedundancy; nbSent++) {
        NS_LOG_INFO("Schedule SendThroughGW after " << emitTime << " seconds, total="<<nbSent<<"/"<<OBJECT_SIZE_BYTES/payloadSize <<" DR: "<<(uint64_t)dr<<" Freq: "<<freq<<" payload size: "<<payloadSize);

        uint32_t fragmentSize = payloadSize; // null bytes padding if too big
        Ptr<Packet> pktPayload = Create<Packet>();

        // add packet fragment number
        oHdr.setFragIndex(0); // TODO: change to have several multicasts in parallel
        oHdr.setFragNumber(nbSent);
        oHdr.setFragmentSize(fragmentSize);

        pktPayload->AddHeader(oHdr);
        pktPayload->AddHeader(fHdr);
        pktPayload->AddHeader(mHdr);
        tag.SetDataRate(dr);
        tag.SetFrequency(freq);
        pktPayload->AddPacketTag(tag);

        Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0))+Seconds(nbSent),
            &NetworkStatus::SendThroughGateway, networkStatus, pktPayload, gwToUse);
    }

    // GO BACK TO INIT PHASE AFTER EVERYTHING IS FINISHED
    NS_LOG_INFO("Go back to init in "<<std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0))+Seconds(2000) << "seconds");
    Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0))+Seconds(2000),
        &ConfirmedMessagesComponent::SwitchToState, this, ObjectPhase::initialize);
}

void
ConfirmedMessagesComponent::BeforeSendingReply(Ptr<EndDeviceStatus> status,
                                               Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);
    // Nothing to do in this case
}

void
ConfirmedMessagesComponent::OnFailedReply(Ptr<EndDeviceStatus> status,
                                          Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << networkStatus);

    // Empty the Ack bit.
    status->m_reply.frameHeader.SetAck(false);
}

////////////////////////
// LinkCheckComponent //
////////////////////////
TypeId
LinkCheckComponent::GetTypeId()
{
    static TypeId tid = TypeId("ns3::LinkCheckComponent")
                            .SetParent<NetworkControllerComponent>()
                            .AddConstructor<LinkCheckComponent>()
                            .SetGroupName("lorawan");
    return tid;
}

LinkCheckComponent::LinkCheckComponent()
{
}

LinkCheckComponent::~LinkCheckComponent()
{
}

void
LinkCheckComponent::OnReceivedPacket(Ptr<const Packet> packet,
                                     Ptr<EndDeviceStatus> status,
                                     Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);

    // We will only act just before reply, when all Gateways will have received
    // the packet.
}

void
LinkCheckComponent::BeforeSendingReply(Ptr<EndDeviceStatus> status,
                                       Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);

    Ptr<Packet> myPacket = status->GetLastPacketReceivedFromDevice()->Copy();
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);

    Ptr<LinkCheckReq> command = fHdr.GetMacCommand<LinkCheckReq>();

    // GetMacCommand returns 0 if no command is found
    if (command)
    {
        status->m_reply.needsReply = true;

        // Get the number of gateways that received the packet and the best
        // margin
        uint8_t gwCount = status->GetLastReceivedPacketInfo().gwList.size();

        Ptr<LinkCheckAns> replyCommand = Create<LinkCheckAns>();
        replyCommand->SetGwCnt(gwCount);
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.AddCommand(replyCommand);
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    }
    else
    {
        // Do nothing
    }
}

void
LinkCheckComponent::OnFailedReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << networkStatus);
}
} // namespace lorawan
} // namespace ns3
