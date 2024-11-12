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

/*
 * This script simulates a simple network in which one end device sends one
 * packet to the gateway.
 */

#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/end-device-lorawan-mac.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/lora-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/one-shot-sender-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/simulator.h"
#include "ns3/ObjectUtility.h"
#include "ns3/point-to-point-module.h"
#include "ns3/buffered-forwarder-helper.h"
#include "ns3/forwarder-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"

#include <algorithm>
#include <ctime>

using namespace ns3;
using namespace lorawan;

#define CURRENT_PERIODICITY 0.001

NS_LOG_COMPONENT_DEFINE("SimpleLorawanNetworkExample");
int SF;
uint32_t packSize;

void logEnergy(DeviceEnergyModelContainer devices) {
    double current = 0;
    for(uint32_t i=0;i<devices.GetN();i++) {
        current = devices.Get(i)->GetCurrentA();
        NS_LOG_INFO("Current: "<<current<<" "<<SF<<" "<<packSize<<" "<<devices.Get(i)->GetTotalEnergyConsumption());
    }
    Simulator::Schedule(Seconds(CURRENT_PERIODICITY), logEnergy, devices);
}

int
main(int argc, char* argv[])
{
    for(SF=0;SF<6;SF++){
        for(packSize = 0; packSize < 50; packSize+=10){
            NS_LOG_INFO("PACKET SIZE "<<packSize);
            // Set up logging
            LogComponentEnable("SimpleLorawanNetworkExample", LOG_LEVEL_ALL);
            //LogComponentEnable("LoraChannel", LOG_LEVEL_INFO);
            LogComponentEnable("LoraPhy", LOG_LEVEL_DEBUG);
            LogComponentEnableAll(LOG_PREFIX_FUNC);
            LogComponentEnableAll(LOG_PREFIX_NODE);
            LogComponentEnableAll(LOG_PREFIX_TIME);

            /************************
            *  Create the channel  *
            ************************/

            NS_LOG_INFO("Creating the channel...");

            // Create the lora channel object
            Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
            loss->SetPathLossExponent(3.76);
            loss->SetReference(1, 7.7);

            Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

            Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

            /************************
            *  Create the helpers  *
            ************************/

            NS_LOG_INFO("Setting up helpers...");

            MobilityHelper mobility;
            Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
            allocator->Add(Vector(1000, 0, 0));
            allocator->Add(Vector(0, 0, 0));
            mobility.SetPositionAllocator(allocator);
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

            // Create the LoraPhyHelper
            LoraPhyHelper phyHelper = LoraPhyHelper();
            phyHelper.SetChannel(channel);

            // Create the LorawanMacHelper
            LorawanMacHelper macHelper = LorawanMacHelper();
            macHelper.SetRegion(LorawanMacHelper::EU);

            // Create the LoraHelper
            LoraHelper helper = LoraHelper();

            /************************
            *  Create End Devices  *
            ************************/

            NS_LOG_INFO("Creating the end device...");

            // Create a set of nodes
            NodeContainer endDevices;
            endDevices.Create(1);

            // Assign a mobility model to the node
            mobility.Install(endDevices);

            // Create the LoraNetDevices of the end devices
            phyHelper.SetDeviceType(LoraPhyHelper::ED);
            macHelper.SetDeviceType(LorawanMacHelper::ED_A);
            auto netdevs = helper.Install(phyHelper, macHelper, endDevices);

            // activate ACK
            for(NetDeviceContainer::Iterator dev = netdevs.Begin(); dev<netdevs.End(); dev++) {
                auto ldev = (*dev)->GetObject<LoraNetDevice>();
                ldev->GetMac()->GetObject<ns3::lorawan::EndDeviceLorawanMac>()->SetMType(ns3::lorawan::LorawanMacHeader::CONFIRMED_DATA_UP);
            }
            /*********************
            *  Create Gateways  *
            *********************/

            NS_LOG_INFO("Creating the gateway...");
            NodeContainer gateways;
            gateways.Create(1);

            mobility.Install(gateways);

            // Create a netdevice for each gateway
            phyHelper.SetDeviceType(LoraPhyHelper::GW);
            macHelper.SetDeviceType(LorawanMacHelper::GW);
            helper.Install(phyHelper, macHelper, gateways);

            /*********************************************
            *  Install applications on the end devices  *
            *********************************************/

            OneShotSenderHelper oneShotSenderHelper;
            oneShotSenderHelper.SetPacketSize(packSize+4);
            oneShotSenderHelper.SetSendTime(Seconds(10));
            oneShotSenderHelper.Install(endDevices);

            /******************
            * Set Data Rates *
            ******************/
            std::vector<double> sfQuantity(6);
            sfQuantity[5-SF] = 1;
            LorawanMacHelper::SetSpreadingFactorsGivenDistribution(endDevices, gateways,sfQuantity);


            ////////////
            // Create network serverNS
            ////////////

            Ptr<Node> networkServer = CreateObject<Node>();

            // PointToPoint links between gateways and server
            PointToPointHelper p2p;
            p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
            p2p.SetChannelAttribute("Delay", StringValue("0ms"));
            // Store network server app registration details for later
            P2PGwRegistration_t gwRegistration;

            for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
            {
                auto container = p2p.Install(networkServer, *gw);
                auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
                gwRegistration.emplace_back(serverP2PNetDev, *gw);
            }

            // Install the NetworkServer application on the network server
            NetworkServerHelper networkServerHelper;
            networkServerHelper.SetGatewaysP2P(gwRegistration);
            networkServerHelper.SetEndDevices(endDevices);
            networkServerHelper.Install(networkServer);

            // Install the Forwarder application on the gateways
            BufferedForwarderHelper forwarderHelper;
            //ForwarderHelper forwarderHelper;

            forwarderHelper.Install(gateways);

            /************************
             * Install Energy Model *
             ************************/

            BasicEnergySourceHelper basicSourceHelper;
            LoraRadioEnergyModelHelper radioEnergyHelper;

            // configure energy source
            basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000)); // Energy in J
            basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));

            radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0076));
            radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0245));
            radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0035));
            radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0076));

            // install source on end devices' nodes
            EnergySourceContainer sources = basicSourceHelper.Install(endDevices);
            //Names::Add("/Names/EnergySource", sources.Get(0));

            // install device model
            DeviceEnergyModelContainer deviceModels =
                radioEnergyHelper.Install(netdevs, sources);


            // Function to log energy consumed
            Simulator::Schedule(Seconds(0), &logEnergy, deviceModels);

            /****************
            *  Simulation  *
            ****************/

            Simulator::Stop(Seconds(15));

            Simulator::Run();

            Simulator::Destroy();
        }
    }

    return 0;
}
