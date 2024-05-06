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

#ifndef BUFFERED_FORWARDER_H
#define BUFFERED_FORWARDER_H

#include "lora-net-device.h"
#include "ns3/application.h"
#include "ns3/attribute.h"
#include "ns3/nstime.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/forwarder.h"

namespace ns3
{
namespace lorawan
{

/**
 * \ingroup lorawan
 *
 * This application forwards packets between NetDevices:
 * LoraNetDevice -> PointToPointNetDevice and vice versa.
 */
class BufferedForwarder : public Forwarder
{
  public:
    BufferedForwarder();           //!< Default constructor
    ~BufferedForwarder() override; //!< Destructor

    /**
     *  Register this type.
     *  \return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Receive a packet from the PointToPointNetDevice and add it to buffer for transmission when possible
     *
     * \copydoc ns3::NetDevice::ReceiveCallback
     */
    virtual bool ReceiveFromPointToPoint(Ptr<NetDevice> device,
                                 Ptr<const Packet> packet,
                                 uint16_t protocol,
                                 const Address& sender);

    /**
     * Forward the packet in head of the buffer when possible or reschedules it later
    */
    void Forward();

  private:
    std::list<Ptr<const Packet>> m_packetBuffer;
};

} // namespace lorawan

} // namespace ns3
#endif /* BUFFERED_FORWARDER_H */
