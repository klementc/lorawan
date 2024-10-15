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
#include "ns3/buffered-forwarder-helper.h"
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
#include "ns3/ObjectCommApplication-multicast.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/file-helper.h"
#include "ns3/okumura-hata-propagation-loss-model.h"

#include "ns3/ObjectUtility.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("BulkCommAppMulticast");

double simTime = 100000;
void logEnergy(DeviceEnergyModelContainer devices) {
    static double prevVal = -1;
    double totEnergy = 0;

    for(uint32_t i=0;i<devices.GetN();i++) {
        totEnergy += devices.Get(i)->GetTotalEnergyConsumption();
        //NS_LOG_INFO(deviceModels.Get(i)->GetTotalEnergyConsumption());
    }
    if (totEnergy != prevVal || std::fmod(Simulator::Now().GetSeconds(),100) == 0 ) {
        NS_LOG_INFO("Total energy: "<<totEnergy);
        prevVal = totEnergy;
    }
    Simulator::Schedule(Seconds(10), logEnergy, devices);
}

int
main(int argc, char* argv[])
{
    bool verbose = false;
    int obj_size = 200;
    int nb_ED = 1;
    int seed = 1;
    double dist = 0;
    double delayReTx = 50;
    double codingRatio = 0.9; // ~10% of error supported
    std::string position = "Fixed"; // "Fixed" for stations at the exact distance dist to the GW, or "Random" for machines at a random distance between 0 and dist

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Whether to print output or not", verbose);
    cmd.AddValue("nb_ED", "Number of end devices in the simulated platform", nb_ED);
    cmd.AddValue("obj_size", "size of the transfered object in bytes", obj_size);
    cmd.AddValue("seed", "random seed", seed);
    cmd.AddValue("dist", "distance between the ED and the GW (x and y maximum distance for Random positionning)", dist);
    cmd.AddValue("delayReTx", "delay between consecutive request after a communication fail", delayReTx);
    cmd.AddValue("position", "Fixed=exactly dist from the GW, Random=random position between 0 and dist to the GW", position);
    cmd.AddValue("CR", "Coding ratio to be used to code the data with redundancy", codingRatio);
    cmd.Parse(argc, argv);


    CR = codingRatio;
    OBJECT_SIZE_BYTES = obj_size;

    ns3::RngSeedManager::SetSeed(seed);
    ns3::RngSeedManager::SetRun(seed);

    // Logging
    //////////
    LogComponentEnable("BulkCommAppMulticast", LOG_LEVEL_ALL);
    LogComponentEnable("ClassAOpenWindowEndDeviceLorawanMac", LOG_LEVEL_INFO);
    LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_INFO);
    LogComponentEnable("ObjectCommApplicationMulticast", LOG_LEVEL_DEBUG);
    LogComponentEnable("NetworkControllerComponent", LOG_LEVEL_DEBUG);
    LogComponentEnable("BufferedForwarder", LOG_LEVEL_ALL);
    //LogComponentEnable("ObjectCommHeader", LOG_LEVEL_ALL);
    //LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
    //LogComponentEnable("NetworkServer", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraFrameHeader", LOG_LEVEL_ALL);
    // LogComponentEnable("LorawanMacHeader", LOG_LEVEL_ALL);
    // LogComponentEnable("MacCommand", LOG_LEVEL_ALL);
    //LogComponentEnable("GatewayLoraPhy", LOG_LEVEL_ALL);
    //LogComponentEnable("LoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraChannel", LOG_LEVEL_ALL);
    //LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
    //LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnable ("OneShotSender", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable ("Forwarder", LOG_LEVEL_ALL);
    // LogComponentEnable ("OneShotSender", LOG_LEVEL_ALL);
    // LogComponentEnable ("DeviceStatus", LOG_LEVEL_ALL);
    //LogComponentEnable ("GatewayStatus", LOG_LEVEL_ALL);
    //LogComponentEnable ("LoraRadioEnergyModel", LOG_LEVEL_ALL);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();

    // Create a simple wireless channel
    ///////////////////////////////////
    Ptr<OkumuraHataPropagationLossModel> loss = CreateObject<OkumuraHataPropagationLossModel>();
    loss->SetAttribute("Frequency", DoubleValue(865*1e6));
    loss->SetAttribute("Environment", EnumValue(EnvironmentType::OpenAreasEnvironment));
    //Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    //loss->SetPathLossExponent(3.76);
    //loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Helpers
    //////////

    // End device mobility
    // Heights of gateway and end devices taken from: Comparing and Adapting Propagation Models for LoRa Network
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    Ptr<ListPositionAllocator> positionAllocEd = CreateObject<ListPositionAllocator>();
    for(int i=0;i<nb_ED;i++) {
        NS_LOG_INFO("Using position: "<<position);
        if (position == "Random")
            positionAllocEd->Add(Vector(rng->GetValue(0, dist), rng->GetValue(0, dist), 15.6));
        else if (position == "Fixed")
            positionAllocEd->Add(Vector(dist, 0, 15.6));
        else {
            NS_LOG_ERROR("PROBLEM: position must be Fixed or Random, user provided"<<position);
        }
    }
    mobilityEd.SetPositionAllocator(positionAllocEd);
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Gateway mobility
    Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator>();
    positionAllocGw->Add(Vector(0.0, 0.0, 1.1));
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
    macHelper.SetDeviceType(LorawanMacHelper::ED_A_OPEN);
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);

    NetDeviceContainer netdevs = helper.Install(phyHelper, macHelper, endDevices);
    for(NetDeviceContainer::Iterator dev = netdevs.Begin(); dev<netdevs.End(); dev++) {
        auto ldev = (*dev)->GetObject<LoraNetDevice>();
        ldev->GetMac()->GetObject<ns3::lorawan::EndDeviceLorawanMac>()->SetMType(ns3::lorawan::LorawanMacHeader::CONFIRMED_DATA_UP);
    }
    ObjectFactory factory;
    factory.SetTypeId("ns3::ObjectCommApplicationMulticast");
    /* Communication in the LoRa zone */
    for (int i=0; i<nb_ED; i++) {
        Ptr<ObjectCommApplicationMulticast> app = factory.Create<ObjectCommApplicationMulticast>();
        app->SetStartTime(Seconds(rng->GetInteger(10, 100)));
        //if (i%2 == 0) app->SetStartTime(Time::FromDouble(rng->GetInteger(10, 100), Time::Unit::S));
        //else          app->SetStartTime(Time::FromDouble(rng->GetInteger(3000, 3000), Time::Unit::S));
        app->SetMCR(codingRatio);
        app->SetNode(endDevices.Get(i));
        app->SetMinDelayReTx(delayReTx);

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
    auto sf = LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
    NS_LOG_INFO("SF INFO: ");
    for (size_t i=0;i<sf.size();i++)
        NS_LOG_INFO("SF INFO "<<i <<" "<<sf.at(i));


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
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000000)); // Energy in J
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

    radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));

    radioEnergyHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                        "TxCurrent",
                                        DoubleValue(0.028));

    // install source on end devices' nodes
    EnergySourceContainer sources = basicSourceHelper.Install(endDevices);
    Names::Add("/Names/EnergySource", sources.Get(0));

    // install device model
    DeviceEnergyModelContainer deviceModels =
        radioEnergyHelper.Install(netdevs, sources);


    // Function to log energy consumed
    Simulator::Schedule(Seconds(0), &logEnergy, deviceModels);

    // Start simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    /**************
     * Get output *
     **************/
    FileHelper fileHelper;
    fileHelper.ConfigureFile("battery-level", FileAggregator::SPACE_SEPARATED);
    fileHelper.WriteProbe("ns3::DoubleProbe", "/Names/EnergySource/RemainingEnergy", "Output");
    Simulator::Destroy();
    return 0;
}
