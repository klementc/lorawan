/*
 * Copyright (c) 2017 University of Padova
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

#include "buffered-forwarder.h"
#include "gateway-lorawan-mac.h"

#include "ns3/log.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("BufferedForwarder");

NS_OBJECT_ENSURE_REGISTERED(BufferedForwarder);

TypeId
BufferedForwarder::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BufferedForwarder")
                            .SetParent<Application>()
                            .AddConstructor<BufferedForwarder>()
                            .SetGroupName("lorawan");
    return tid;
}

BufferedForwarder::BufferedForwarder()
{
    NS_LOG_FUNCTION_NOARGS();
}

BufferedForwarder::~BufferedForwarder()
{
    NS_LOG_FUNCTION_NOARGS();
}

void BufferedForwarder::Forward() {
    NS_LOG_FUNCTION_NOARGS();

    Ptr<Packet> packet = m_packetBuffer.back()->Copy();
    LoraTag tag;
    packet->RemovePacketTag(tag);
    uint8_t dataRate = tag.GetDataRate();
    double frequency = tag.GetFrequency();
    packet->AddPacketTag(tag);

    // take head of list and send it if possible
    NS_LOG_DEBUG("Packet to send on frequency: "<<frequency<<" with datarate: "<< (uint64_t)dataRate);
    Time waitTime = m_loraNetDevice->GetMac()->GetObject<GatewayLorawanMac>()->GetWaitingTime(tag.GetFrequency());
    NS_LOG_DEBUG("With waiting time "<<waitTime);
    // if no waiting time, send now
    if ( waitTime<=Seconds(0)) {
        NS_LOG_DEBUG("Send tail packet, buffer size "<<m_packetBuffer.size());

        Ptr<Packet> packetCopy = m_packetBuffer.back()->Copy();
        m_packetBuffer.pop_back();
        m_loraNetDevice->Send(packetCopy);
    }  // if not possible reschedule for later
    else {
        NS_LOG_DEBUG("Cannot send now, schedule after "<<waitTime<<"s.");

        Simulator::Schedule(waitTime, &BufferedForwarder::Forward, this);
    }
}

bool
BufferedForwarder::ReceiveFromPointToPoint(Ptr<NetDevice> pointToPointNetDevice,
                                   Ptr<const Packet> packet,
                                   uint16_t protocol,
                                   const Address& sender)
{
    m_packetBuffer.push_back(packet);
    NS_LOG_DEBUG("Buffer size: "<<m_packetBuffer.size());
    Forward();

    return true;
}

} // namespace lorawan
} // namespace ns3
