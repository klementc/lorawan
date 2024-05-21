#include "class-a-openwindow-end-device-lorawan-mac.h"

#include "end-device-lora-phy.h"
#include "end-device-lorawan-mac.h"

#include "ns3/log.h"

#include <algorithm>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ClassAOpenWindowEndDeviceLorawanMac");

NS_OBJECT_ENSURE_REGISTERED(ClassAOpenWindowEndDeviceLorawanMac);

TypeId
ClassAOpenWindowEndDeviceLorawanMac::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ClassAOpenWindowEndDeviceLorawanMac")
                            .SetParent<ClassAEndDeviceLorawanMac>()
                            .SetGroupName("lorawan")
                            .AddConstructor<ClassAOpenWindowEndDeviceLorawanMac>();
    return tid;
}

ClassAOpenWindowEndDeviceLorawanMac::ClassAOpenWindowEndDeviceLorawanMac()
    : ClassAEndDeviceLorawanMac(),
      m_isInOpenWindow(false)
{
    NS_LOG_FUNCTION(this);
}

ClassAOpenWindowEndDeviceLorawanMac::~ClassAOpenWindowEndDeviceLorawanMac()
{
    NS_LOG_FUNCTION_NOARGS();
}

void ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow(double frequency, uint8_t datarate) {
    NS_LOG_FUNCTION(frequency << datarate);
    m_fr = frequency;
    m_sf = datarate;
    // Set Phy in Standby mode
    m_phy->GetObject<EndDeviceLoraPhy>()->SwitchToStandby();

    m_phy->GetObject<EndDeviceLoraPhy>()->SetFrequency(m_fr);
    m_phy->GetObject<EndDeviceLoraPhy>()->SetSpreadingFactor(GetSfFromDataRate(m_sf));

    m_isInOpenWindow = true;
    // for now just stay in standby until further notice. TODO: compute when to close it in case there was a problem
}

void ClassAOpenWindowEndDeviceLorawanMac::closeFreeReceiveWindow() {
  NS_LOG_FUNCTION_NOARGS();
  Ptr<EndDeviceLoraPhy> phy = m_phy->GetObject<EndDeviceLoraPhy>();
  m_isInOpenWindow = false;

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
  case EndDeviceLoraPhy::STANDBY:
      // Turn PHY layer to sleep
      phy->SwitchToSleep();
      break;
  case EndDeviceLoraPhy::RX:
      // PHY is receiving: let it finish
      //NS_LOG_INFO("PHY is receiving: Receive will handle the result.");
      return;
  }

}

void ClassAOpenWindowEndDeviceLorawanMac::StartWindowInterruption(double duration) {
    NS_LOG_FUNCTION(duration);
    resetRetransmissionParameters();
    Simulator::Cancel(m_secondReceiveWindow);
    if(m_phy->GetObject<EndDeviceLoraPhy>()->GetState()==EndDeviceLoraPhy::State::RX) return;
    closeFreeReceiveWindow();
    Simulator::Schedule(Seconds(duration), &ClassAOpenWindowEndDeviceLorawanMac::openFreeReceiveWindow, this, m_fr, m_sf);
}

bool ClassAOpenWindowEndDeviceLorawanMac::checkIsInOpenSlot() {
    return m_isInOpenWindow;
}
} /* namespace lorawan */
} /* namespace ns3 */
