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

double simTime = 1000000;
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
    int obj_size = 200;
    int nb_ED = 4;
    int seed = 1;
    double delayReTx = 50;
    double codingRatio = 0.9; // ~10% of error supported
    std::string position = "Fixed"; // "Fixed" for stations at the exact distance dist to the GW, or "Random" for machines at a random distance between 0 and dist
    std::string policy = "ALL";
    int singleRound = 1;
    int useRNG = 0;


    double fbu_freq = 0;
    uint8_t fbu_dr = 10;
    double fbu_plsize = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("obj_size", "size of the transfered object in bytes", obj_size);
    cmd.AddValue("nb_ED","nb of end devices", nb_ED);
    cmd.AddValue("seed", "random seed", seed);
    cmd.AddValue("delayReTx", "delay between consecutive request after a communication fail", delayReTx);
    cmd.AddValue("CR", "Coding ratio to be used to code the data with redundancy", codingRatio);
    cmd.AddValue("duration","duration of the simulation", simTime);
    cmd.AddValue("policy", "ALL|FASTEST|THRESHOLD",policy);
    cmd.AddValue("thresholdUpdate","Time in days before a mandatory update (THRESHOLD policy only)",thresholdUpdate);
    cmd.AddValue("singleRound","Stop applications after a single round of update (1/0)", singleRound);
    cmd.AddValue("useRNG","de/activate random delays between failed uplink messages", useRNG);

    cmd.AddValue("fbu_freq","SET ONLY IF USING FIXED_BY_USER CONFIG: frequency to use ", fbu_freq);
    cmd.AddValue("fbu_dr","SET ONLY IF USING FIXED_BY_USER CONFIG: Datarate to use for the futoa ", fbu_dr);
    cmd.AddValue("fbu_plsize","SET ONLY IF USING FIXED_BY_USER CONFIG: payload size to use for the fuota ", fbu_plsize);

    cmd.AddValue("EMIT_DELAY","EMIT_DELAY",EMIT_DELAY);
    cmd.AddValue("DUR_POOLING","DUR_POOLING",DUR_POOLING);
    cmd.AddValue("inter_FUOTA_interval","inter_FUOTA_interval",inter_FUOTA_interval);
    cmd.Parse(argc, argv);


    CR = codingRatio;
    OBJECT_SIZE_BYTES = obj_size;

    ns3::RngSeedManager::SetSeed(seed);
    ns3::RngSeedManager::SetRun(seed);

    // Logging
    //////////
    LogComponentEnable("BulkCommAppMulticast", LOG_LEVEL_ALL);
    LogComponentEnable("ObjectCommApplicationMulticast", LOG_LEVEL_ALL);
    LogComponentEnable("NetworkControllerComponent", LOG_LEVEL_INFO);
    LogComponentEnable("ClassAOpenWindowEndDeviceLorawanMac", LOG_LEVEL_INFO);
    //LogComponentEnable("LoraPhy", LOG_LEVEL_DEBUG);

    LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_DEBUG);
    //LogComponentEnable("BufferedForwarder", LOG_LEVEL_DEBUG);
    LogComponentEnable("LorawanMacHelper", LOG_LEVEL_INFO);

    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    NS_LOG_INFO("WITH POLICY '"<<policy.c_str()<<"'");
    if (policy.compare("ALL")==0)
        SELECTED_POLICY = txParamsPolicy::ALL_MACHINES;
    else if (policy.compare("FUOTA_BASELINE")==0)
        SELECTED_POLICY = txParamsPolicy::FUOTA_BASELINE;
    else if (policy=="FASTEST")
        SELECTED_POLICY = txParamsPolicy::FASTEST_MACHINES;
    else if (policy =="THRESHOLD2") {
        SELECTED_POLICY = txParamsPolicy::THRESHOLD;
        thresholdUpdate = 10000; // fixed by me
    }
    else if (policy =="THRESHOLD3") {
        SELECTED_POLICY = txParamsPolicy::THRESHOLD;
        thresholdUpdate = 16000; // fixed by me
    }
    else if (policy == "FIXED_BY_USER") {
        SELECTED_POLICY = txParamsPolicy::FIXED_BY_USER;
        // set all parameters for the header based on cli args of the user
        FIXED_BY_USER_DR = fbu_dr;
        FIXED_BY_USER_FREQ = fbu_freq;
        FIXED_BY_USER_PLSIZE = fbu_plsize;
    } else {
        NS_ABORT_MSG("SPECIFY A CORRECT POLICY");
        exit(1);
    }
    NS_LOG_INFO("Using policy "<<SELECTED_POLICY);

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
    NodeContainer endDevices;
    endDevices.Create(nb_ED);
    NodeContainer gateways;
    gateways.Create(1);

    // End device mobility
    // Heights of gateway and end devices taken from: Comparing and Adapting Propagation Models for LoRa Network
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    Ptr<ListPositionAllocator> positionAllocEd = CreateObject<ListPositionAllocator>();

    for(int i=0;i<nb_ED;i++) {
        double r = 0;
        double theta = rng->GetValue(0,1) * 2 * 3.14159265358979323846;
        NS_LOG_INFO("Using position: "<<position);

        r = 19000 * sqrt(rng->GetValue(0,1));
        double x = r * cos(theta);
        double y = r * sin(theta);
        positionAllocEd->Add(Vector(x, y, 1.1));
        NS_LOG_INFO("Add STA in ( "<< x <<" , "<<y<<" , 0 ) STA"<<endDevices.Get(i)->GetId());
    }
    mobilityEd.SetPositionAllocator(positionAllocEd);
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Gateway mobility
    Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator>();
    {
        double x, y;
        x = 0;
        y = 0;
        positionAllocGw->Add(Vector(x, y, 15.6));
        NS_LOG_INFO("Add GW in ( "<<x<<" , "<<y<<" , 0 ) GW"<<gateways.Get(0)->GetId());
    }
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
        double delay = useRNG==1 ? rng->GetInteger(10, 1000) : 10+(i*5);
        app->SetStartTime(Seconds(delay));
        app->SetMCR(codingRatio);
        app->SetNode(endDevices.Get(i));
        app->SetMinDelayReTx(delayReTx);
        if (useRNG==0)
            app->CancelRNG();
        if (singleRound)
            app->setSingleUpdate();

        endDevices.Get(i)->AddApplication(app);
    }

    ////////////////
    // Create gateways //
    ////////////////
    mobilityGw.Install(gateways);

    // Create the LoraNetDevices of the gateways
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    // Set spreading factors up
    std::vector<int> sf;

    std::vector<double> sfQuantity = {1./6, 1./6, 1./6, 1./6, 1./6, 1./6};
    //sf = LorawanMacHelper::SetSpreadingFactorsGivenDistribution(endDevices, gateways,sfQuantity);
    sf = LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
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

    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000)); // Energy in J
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));

    radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0076));
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0245));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0076));


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
