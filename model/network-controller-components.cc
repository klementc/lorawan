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

    // if object broadcast request process it, otherwise keep classic ACK for confirmed packets
    if (fHdr.GetFPort() == 13){
        ProcessPacket(packet, status, networkStatus);
    } else if (mHdr.GetMType() == LorawanMacHeader::CONFIRMED_DATA_UP)
    {
        NS_LOG_INFO("Packet requires confirmation");

        // Set up the ACK bit on the reply
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.SetAck(true);
        status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;
        status->m_reply.payload = Create<Packet>(230);
        //status->m_reply.payload = Create<Packet>(0);

        // Note that the acknowledgment procedure dies here: "Acknowledgments
        // are only snt in response to the latest message received and are never
        // retransmitted". We interpret this to mean that only the current
        // reception window can be used, and that the Ack field should be
        // emptied in case transmission cannot be performed in the current
        // window. Because of this, in this component's OnFailedReply method we
        // void the ack bits.
    }
}

static bool alreadySent = false;
double freq = 0;
uint8_t dr = 10;
double emitTime = 0;
std::pair<ConfirmedMessagesComponent::ObjectPhase, double> currentState = std::make_pair(ConfirmedMessagesComponent::ObjectPhase::initialize, 0);
LorawanMacHeader mHdr;
LoraFrameHeader fHdr;
ObjectCommHeader oHdr;
LoraTag tag;
LoraDeviceAddress devAddr;
void ConfirmedMessagesComponent::ProcessPacket(Ptr<const Packet> packet,
                                             Ptr<EndDeviceStatus> status,
                                             Ptr<NetworkStatus> networkStatus)
{

    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);

    // Process in case of single fragment request
    Ptr<Packet> checkType = packet->Copy();
    LorawanMacHeader mHdrtmp; LoraFrameHeader fHdrtmp; ObjectCommHeader oHdrtmp;
    checkType->RemoveHeader(mHdrtmp);
    checkType->RemoveHeader(fHdrtmp);
    checkType->RemoveHeader(oHdrtmp);
    if (oHdrtmp.GetType()==3) {
        NS_LOG_INFO("Single Fragment Tx: # "<<oHdr.GetFragmentNumber());
        // create ACK reply with payload
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.SetAck(true);
        status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;

        LoraTag tagtmp;
        checkType->RemovePacketTag(tagtmp);
        int drVal = -1;
        for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==tagtmp.GetSpreadingFactor()) {drVal=i;break;}}
        // TODO: compute size for last packet which can be smaller than that
        auto retFragment = Create<Packet>(maxPLsize[drVal]-oHdr.GetSerializedSize());
        oHdrtmp.SetType(3);
        retFragment->AddHeader(oHdrtmp);
        status->m_reply.payload = retFragment;
        return; // dont go further, no need to
    }


    if(currentState.first == ObjectPhase::pool && currentState.second+1000<=(Simulator::Now()).GetSeconds()) {
        // must switch to advertising phase
        currentState = std::make_pair(ObjectPhase::advertize, Simulator::Now().GetSeconds());
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
        double drtmp = 0;
        for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==tagtmp.GetSpreadingFactor()) {drtmp=i;break;}}
        if (drtmp<dr) {
            NS_LOG_INFO("SET NEW SF VALUE IN POOL PHASE: "<<(uint64_t)dr<<"->"<<drtmp);
            //
            myPacket->RemoveHeader(mHdr);
            myPacket->RemoveHeader(fHdr);
            myPacket->RemoveHeader(oHdr);
            devAddr = fHdr.GetAddress();
            tag = tagtmp;
            dr=drtmp;
        }
    } else if (currentState.first == ObjectPhase::advertize) {
        // Advertise, compute the SF and Freq is necessary, and send an ACK
        NS_LOG_INFO("ADVERTISING");
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.frameHeader.SetAck(true);
        status->m_reply.frameHeader.SetAddress(fHdr.GetAddress());
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;
        auto retPL = Create<Packet>(0);

        if (! alreadySent) {
            //take freq and sf from LoRa tag
            freq = tag.GetFrequency();
            dr = 0;
            for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==tag.GetSpreadingFactor()) {dr=i;break;}}
            NS_LOG_INFO("freq: "<<tag.GetFrequency()<<" header index: "<< ObjectCommHeader::GetFrequencyIndex(freq)<< "SFFFFFFFFFFFFFFF: "<<(uint64_t)tag.GetSpreadingFactor() << " DR: "<<(uint64_t)tag.GetDataRate() << " corrected DR: "<<(uint64_t)dr);
            emitTime = (Simulator::Now()+Seconds(1000)).GetSeconds();
        }

        oHdr.SetFreq(ObjectCommHeader::GetFrequencyIndex(freq));
        oHdr.SetDR(dr);
        auto plSize = 0.;
        oHdr.SetType(2); // just to get the size of the header with fragment
        for(long unsigned int i=0;i<sfdr.size();i++){if(sfdr[i]==tag.GetSpreadingFactor()) {plSize = maxPLsize[i]-oHdr.GetSerializedSize();break;}}
        oHdr.SetType(1); // DL ACK type
        NS_LOG_INFO("plsize: "<<plSize<<" fn: "<<OBJECT_SIZE_BYTES/plSize<< " OS: "<<OBJECT_SIZE_BYTES);
        oHdr.SetFragmentNumber(std::ceil( OBJECT_SIZE_BYTES/plSize));

        uint8_t nbTicks = (uint8_t)(floor((Seconds(emitTime)-Simulator::Now()).GetSeconds()/10))-1;
        // special case cause nbTicks can underflow and lead to unwanted value, cancel because you wont have time to send the ACK
        if ((floor((Seconds(emitTime)-Simulator::Now()).GetSeconds()/10))-1 < 0) {
            status->m_reply.frameHeader.SetAck(false);
            status->m_reply.needsReply = false;
        }
        NS_LOG_INFO("NB TICKS: "<<emitTime<<" "<<(floor(((Seconds(emitTime)-Simulator::Now()).GetSeconds())/10)));
        oHdr.SetDelay(nbTicks);

        retPL->AddHeader(oHdr);
        status->m_reply.payload = retPL;

        NS_LOG_INFO("Request for object ID: "<< (uint64_t)oHdr.GetObjID()<< " already sent: "<<alreadySent);
        if(! alreadySent) {
            //Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0)),
            //    &ConfirmedMessagesComponent::EmitObject, this, networkStatus->GetReplyForDevice(devAddr, 3), networkStatus);
            EmitObject(networkStatus->GetReplyForDevice(devAddr, 3), networkStatus);
            alreadySent = true;
        }
    }
    // possibly to use to retransmit the object after a tramsnission was already started
    /*if (alreadySent && Simulator::Now()>Seconds(emitTime)) {
        NS_LOG_INFO("!!!!!!!!!!WARNING: CURRENTLY WORKS ONLY IF OBJECT TX DUR < 1000");
        NS_LOG_INFO("OBJECT ALREADY SENT (OR CURRENTLY SENDING), RESET TX PARAMS FOR FUTURE BROADCAST");
        alreadySent=false;
    }*/

}

void ConfirmedMessagesComponent::SwitchToState(ObjectPhase phase){
    currentState = std::make_pair(phase, Simulator::Now().GetSeconds());
    // if we reset the alg, we need to reset alreadySent
    if (phase == ObjectPhase::initialize) alreadySent = false;
}

void ConfirmedMessagesComponent::EmitObject(Ptr<Packet> packetTemplate, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_INFO("CALL TO EMIT");
    // switch to sending since we start sending now

    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    ObjectCommHeader oHdr;
    LoraTag tag;

    Ptr<Packet> pcktdata = packetTemplate->Copy();
    pcktdata->RemoveHeader(mHdr);
    pcktdata->RemoveHeader(fHdr);
    pcktdata->RemovePacketTag(tag);
    auto gwToUse = networkStatus->GetBestGatewayForDevice(fHdr.GetAddress(), 1);
    fHdr.SetAddress(LoraDeviceAddress(0,0));

    // check if need to ACK or not
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
    oHdr.SetType(2);
    oHdr.SetFreq(oHdr.GetFrequencyIndex(freq));
    oHdr.SetDR(dr);

    int payloadSize = maxPLsize[dr]-oHdr.GetSerializedSize();
    NS_LOG_UNCOND("PL SIZE: "<<payloadSize);

    Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0)),
        &ConfirmedMessagesComponent::SwitchToState, this, ObjectPhase::send);

    for(int nbSent=0;nbSent<OBJECT_SIZE_BYTES; nbSent+=payloadSize) {
        NS_LOG_INFO("Schedule SendThroughGW after " << emitTime << " seconds, total="<<nbSent<<"/"<<OBJECT_SIZE_BYTES/payloadSize);
        NS_LOG_INFO("ADDR: "<< fHdr.GetAddress());

        Ptr<Packet> pktPayload = Create<Packet>(std::min(OBJECT_SIZE_BYTES-nbSent, payloadSize));
        // add packet fragment number
        oHdr.SetFragmentNumber(nbSent/payloadSize);

        pktPayload->AddHeader(oHdr);
        pktPayload->AddHeader(fHdr);
        pktPayload->AddHeader(mHdr);
        tag.SetDataRate(dr);
        //tag.SetSpreadingFactor(sf);
        tag.SetFrequency(freq);
        pktPayload->AddPacketTag(tag);


        Simulator::Schedule(std::max(Seconds(emitTime)-Simulator::Now(),Seconds(0))+Seconds(nbSent/payloadSize),
            &NetworkStatus::SendThroughGateway, networkStatus, pktPayload, gwToUse);
        //networkStatus->SendThroughGateway(pktPayload, gwToUse);
    }

    // TODO: GO BACK TO INIT PHASE AFTER EVERYTHING IS FINISHED
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
