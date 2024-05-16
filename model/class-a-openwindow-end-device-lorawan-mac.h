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
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 *
 * Modified by: Peggy Anderson <peggy.anderson@usask.ca>
 */

#ifndef CLASS_A_OPENWINDOW_END_DEVICE_LORAWAN_MAC_H
#define CLASS_A_OPENWINDOW_END_DEVICE_LORAWAN_MAC_H

#include "lorawan-mac.h"
#include "end-device-lorawan-mac.h" // EndDeviceLorawanMac
#include "lora-frame-header.h"      // RxParamSetupReq
#include "lorawan-mac.h"            // Packet
#include "lora-device-address.h"
#include "class-a-end-device-lorawan-mac.h"

#include "ns3/traced-value.h"

namespace ns3
{
namespace lorawan
{

/**
 * \ingroup lorawan
 *
 * Class representing the MAC layer of a Class A LoRaWAN device.
 */
class ClassAOpenWindowEndDeviceLorawanMac : public ClassAEndDeviceLorawanMac
{
  public:
    /**
     *  Register this type.
     *  \return The object TypeId.
     */
    static TypeId GetTypeId();

    ClassAOpenWindowEndDeviceLorawanMac();           //!< Default constructor
    ~ClassAOpenWindowEndDeviceLorawanMac() override; //!< Destructor

    //////////////////////////
    //  Receiving methods   //
    //////////////////////////
    void openFreeReceiveWindow(double frequency, uint8_t spreadingFactor);
    void closeFreeReceiveWindow();
    bool checkIsInOpenSlot();

  private:
    bool m_isInOpenWindow;
}; /* ClassAEndDeviceLorawanMac */
} /* namespace lorawan */
} /* namespace ns3 */
#endif /* CLASS_A_OPENWINDOW_END_DEVICE_LORAWAN_MAC_H */
