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
 * This example creates a simple network in which all LoRaWAN components are
 * simulated: end devices, some gateways and a network server.
 * Two end devices are already configured to send unconfirmed and confirmed messages respectively.
 */

#include "ns3/command-line.h"
#include "ns3/core-module.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/one-shot-sender-helper.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/periodic-sender.h"
#include "ns3/point-to-point-module.h"
#include "ns3/string.h"
#include "ns3/BulkSendApplication.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/file-helper.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("NetworkServerExample");

int
main(int argc, char* argv[])
{
    bool verbose = false;
    int data_to_send = 1000;
    int nb_ED = 1;
    int seed = 1;
    double dist = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Whether to print output or not", verbose);
    cmd.AddValue("nb_ED", "Number of end devices in the simulated platform", nb_ED);
    cmd.AddValue("data_to_send", "size of the transfered object in bytes", data_to_send);
    cmd.AddValue("seed", "random seed", seed);
    cmd.AddValue("dist", "distance between the ED and the GW", dist);

    cmd.Parse(argc, argv);

    ns3::RngSeedManager::SetSeed(seed);
    ns3::RngSeedManager::SetRun(seed);

    // Logging
    //////////
    //ns3::LogComponentEnable("LoraChannel", ns3::LOG_LEVEL_ALL);

    //LogComponentEnable("NetworkServerExample", LOG_LEVEL_ALL);
    //LogComponentEnable("NetworkServer", LOG_LEVEL_ALL);
    //LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraFrameHeader", LOG_LEVEL_ALL);
    // LogComponentEnable("LorawanMacHeader", LOG_LEVEL_ALL);
    // LogComponentEnable("MacCommand", LOG_LEVEL_ALL);
    // LogComponentEnable("GatewayLoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraChannel", LOG_LEVEL_ALL);
    // LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
    //LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
    //LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnable ("OneShotSender", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable ("Forwarder", LOG_LEVEL_ALL);
    // LogComponentEnable ("OneShotSender", LOG_LEVEL_ALL);
    // LogComponentEnable ("DeviceStatus", LOG_LEVEL_ALL);
    // LogComponentEnable ("GatewayStatus", LOG_LEVEL_ALL);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    // Create a simple wireless channel
    ///////////////////////////////////

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Helpers
    //////////

    // End device mobility
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    Ptr<ListPositionAllocator> positionAllocEd = CreateObject<ListPositionAllocator>();
    for(int i=0;i<nb_ED;i++) {
        positionAllocEd->Add(Vector(dist, 0.0, 0.0));
    }
    mobilityEd.SetPositionAllocator(positionAllocEd);
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Gateway mobility
    Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator>();
    positionAllocGw->Add(Vector(0.0, 0.0, 0.0));
    mobilityGw.SetPositionAllocator(positionAllocGw);
    mobilityGw.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LoraHelper
    LoraHelper helper = LoraHelper();

    // Create end devices
    /////////////

    NodeContainer endDevices;
    endDevices.Create(nb_ED);
    mobilityEd.Install(endDevices);

    // Create a LoraDeviceAddressGenerator
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);

    NetDeviceContainer netdevs = helper.Install(phyHelper, macHelper, endDevices);
    for(NetDeviceContainer::Iterator dev = netdevs.Begin(); dev<netdevs.End(); dev++) {
        auto ldev = (*dev)->GetObject<LoraNetDevice>();
        ldev->GetMac()->GetObject<ns3::lorawan::EndDeviceLorawanMac>()->SetMType(ns3::lorawan::LorawanMacHeader::CONFIRMED_DATA_UP);
    }
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    ObjectFactory factory;
    factory.SetTypeId("ns3::BulkSendApplication");
    /* Communication in the LoRa zone */
    for (int i=0; i<nb_ED; i++) {
        Ptr<BulkSendApplication> app = factory.Create<BulkSendApplication>();

        app->SetStartTime(Time::FromDouble(rng->GetInteger(10, 100), Time::Unit::S));
        app->setDataToSend(data_to_send);
        app->setRequestSize(222);
        app->SetNode(endDevices.Get(i));
        endDevices.Get(i)->AddApplication(app);
    }

    ////////////////
    // Create gateways //
    ////////////////

    NodeContainer gateways;
    gateways.Create(1);
    mobilityGw.Install(gateways);

    // Create the LoraNetDevices of the gateways
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    // Set spreading factors up
    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

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
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);


    /************************
     * Install Energy Model *
     ************************/

    BasicEnergySourceHelper basicSourceHelper;
    LoraRadioEnergyModelHelper radioEnergyHelper;

    // configure energy source
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000000)); // Energy in J
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

    radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));

    // install source on end devices' nodes
    EnergySourceContainer sources = basicSourceHelper.Install(endDevices);
    Names::Add("/Names/EnergySource", sources.Get(0));

    /**************
     * Get output *
     **************/
    FileHelper fileHelper;
    fileHelper.ConfigureFile("battery-level", FileAggregator::SPACE_SEPARATED);
    fileHelper.WriteProbe("ns3::DoubleProbe", "/Names/EnergySource/RemainingEnergy", "Output");

    // Start simulation
    Simulator::Stop(Seconds(1000000));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
