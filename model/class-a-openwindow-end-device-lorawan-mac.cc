#include "class-a-openwindow-end-device-lorawan-mac.h"

#include "end-device-lora-phy.h"
#include "end-device-lorawan-mac.h"

#include "ns3/log.h"


namespace ns3
{
namespace lorawan
{
void ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow(double frequency, uint8_t spreadingFactor) {
  NS_LOG_FUNCTION_NOARGS();
    // Set Phy in Standby mode
    m_phy->GetObject<EndDeviceLoraPhy>()->SwitchToStandby();

    m_phy->GetObject<EndDeviceLoraPhy>()->SetFrequency(frequency);
    m_phy->GetObject<EndDeviceLoraPhy>()->SetSpreadingFactor(
        GetSfFromDataRate(spreadingFactor));

    // for now just stay in standby until further notice. TODO: compute when to close it in case there was a problem
}

void ClassAOpenWindowEndDeviceLorawanMac::closeFreeReceiveWindow() {
  NS_LOG_FUNCTION_NOARGS();

  Ptr<EndDeviceLoraPhy> phy = m_phy->GetObject<EndDeviceLoraPhy>();

  // NS_ASSERT (phy->m_state != EndDeviceLoraPhy::TX &&
  // phy->m_state != EndDeviceLoraPhy::SLEEP);

  // Check the Phy layer's state:
  // - RX -> We have received a preamble.
  // - STANDBY -> Nothing was detected.
  switch (phy->GetState())
  {
  case EndDeviceLoraPhy::TX:
      break;
  case EndDeviceLoraPhy::SLEEP:
      break;
  case EndDeviceLoraPhy::RX:
      // PHY is receiving: let it finish
      NS_LOG_DEBUG("PHY is receiving: Receive will handle the result.");
      return;
  case EndDeviceLoraPhy::STANDBY:
      // Turn PHY layer to sleep
      phy->SwitchToSleep();
      break;
  }

}

} /* namespace lorawan */
} /* namespace ns3 */
