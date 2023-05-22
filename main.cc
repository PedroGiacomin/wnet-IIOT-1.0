// Network topology
//  - Rede 200 x 200 com 1 AP e 2 nodes em um grid.
//  - Nodes distantes 10m um do outro 
//  - Max de 5 nodes por linha
//
// - Servidor em n0, clientes em n1 ... n21
// - Todos os clientes pingam no servidor e em todos os outros clientes, um de cada vez

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("WnetIIOTv1");

int main (int argc, char *argv[]){
    uint16_t numNodes = 5;
    std::string animFile = "wnetIIOTv1-animation.xml";

    LogComponentEnable ("WnetIIOTv1", LOG_LEVEL_INFO);

    NS_LOG_INFO ("Create nodes.");
    NodeContainer nodes, apNode; //apNode salva o 'servidor'
    nodes.Create(numNodes);
    apNode.Create(1);
    NodeContainer allNodes(nodes, apNode); 

    NS_LOG_INFO("Set up mobility.");

    double distNodes = ceil(sqrt(40000/numNodes)); 
	double nodesPerLine = ceil(200/distNodes);

    MobilityHelper nodesMobility;
    nodesMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    nodesMobility.SetPositionAllocator("ns3::GridPositionAllocator",    
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distNodes),
        "DeltaY", DoubleValue(distNodes),
        "GridWidth", UintegerValue(nodesPerLine),
        "LayoutType", StringValue("RowFirst")
    );
    nodesMobility.Install(nodes);

    double apPosX = 100, apPosY = 100;
    MobilityHelper apMobilityHelper;
    apMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobilityHelper.Install(apNode);
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
	apMobility->SetPosition(Vector(apPosX, apPosY, 0.0));

    NS_LOG_INFO("Set up and install LR-WPAN (layers 1 and 2)");
    LrWpanHelper lrwpan;
    NetDeviceContainer wpanDevices = lrwpan.Install(allNodes);
    lrwpan.AssociateToPan(wpanDevices, 0); // associate nodes to the same PAN (id=0)

    NS_LOG_INFO("Set up and install 6lowpan (layer 2)");
    SixLowPanHelper sixlowpan; //6lowpan allows LR-WPAN to use IPv6
    NetDeviceContainer netDevices = sixlowpan.Install(wpanDevices);

    NS_LOG_INFO("Install internet protocols stack.");
    InternetStackHelper internet;
    internet.SetIpv4StackInstall(false);
	internet.SetIpv6StackInstall(true);
	internet.Install(allNodes);

    NS_LOG_INFO("Create and install IPv6 adresses.");
    Ipv6AddressHelper ipv6;
  	//ipv6.SetBase (Ipv6Address ("2020:1::"), Ipv6Prefix (64));
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  	Ipv6InterfaceContainer subnet1 = ipv6.Assign(netDevices);  //Cria rede com IP global 2001:1:0:0:x:x:x:x

    //Testing the IPv6 adresses, getting the address directly from the nodes
    auto apAddress = apNode.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();
    NS_LOG_INFO("AP: IP " << apAddress);
    for (size_t i = 0; i < nodes.GetN(); i++){
		Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
        Ipv6InterfaceAddress iaddr = ipv6->GetAddress(1, 0);
        Ipv6Address llipAddr = iaddr.GetAddress();
        iaddr = ipv6->GetAddress(1, 1);
        Ipv6Address glipAddr = iaddr.GetAddress();

		NS_LOG_INFO("Node " << i << ": \tll-IP " << llipAddr << "\tgl-IP " << glipAddr);
	}

    // NS_LOG_INFO("Setup and install ping6 applications.");
    // uint32_t packetSize = 19;
    // uint32_t maxPacketCount = 1;
    // Time interPacketInterval = Seconds (1.);

    //ping node0 -> AP
    // int i = 0;
    // Ping6Helper pingAP;
    // Ptr<Ipv6> iNode = nodes.Get(i)->GetObject<Ipv6>();
    // Ipv6InterfaceAddress iaddr = iNode->GetAddress(1,0);
    // Ipv6Address nodeAddr = iaddr.GetAddress(); //node 0
    
    // pingAP.SetLocal(nodeAddr);
    // pingAP.SetRemote(apAddress);
    // pingAP.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    // pingAP.SetAttribute ("Interval", TimeValue (interPacketInterval));
    // pingAP.SetAttribute ("PacketSize", UintegerValue (packetSize));

    // ApplicationContainer apps = pingAP.Install(nodes.Get(0)); //instala no node 0
    // apps.Start(Seconds(2.0));
    // apps.Stop(Seconds(10.0));

    //auto apAddress = apNode.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();
    //NS_LOG_INFO("AP: " << apAddress);
    // for (size_t i = 0; i < nodes.GetN(); i++){
	// 	Ptr<MobilityModel> deviceMobility = nodes.Get(i)->GetObject<MobilityModel>();
	// 	double distance = deviceMobility->GetDistanceFrom(apMobility);

	// 	Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
    //     Ipv6InterfaceAddress iaddr = ipv6->GetAddress(1, 0);
    //     Ipv6Address ipAddr = iaddr.GetAddress();

	// 	NS_LOG_INFO("N: IP " << ipAddr << " D " << distance);
	// }


    // --------- PARTE DO RANGE ----------//
    // NS_LOG_INFO("Set AP range.");
    // Ptr<Node> ap = apNode.Get(0);
	// Ptr<LrWpanNetDevice> apnetdev = DynamicCast<LrWpanNetDevice>(ap->GetDevice(1)); //Faz cast de NetDevice para LrWpanNetDevice, mas pq pega o 1?
    // auto apphy = apnetdev->GetPhy();
    // NS_LOG_INFO("Aqui");
	// LrWpanSpectrumValueHelper svh;
	// Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity (9, 11); //Range of 200m according to lr-wpan-error-distance-plot
	// apphy->SetTxPowerSpectralDensity(psd);
    
    // NS_LOG_INFO("Set nodes range.");
    // for (size_t i = 0; i < nodes.GetN(); i++){
    //     Ptr<LrWpanNetDevice> nodenetdev = DynamicCast<LrWpanNetDevice>(nodes.Get(i)->GetDevice(1));
	// 	auto phy = nodenetdev->GetPhy();
	// 	LrWpanSpectrumValueHelper svh;
	// 	Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity(-10, 11); //Range of 50m according to lr-wpan-error-distance-plot
	// 	phy->SetTxPowerSpectralDensity(psd);
    // }
    // --------- DUVIDA ----------//

    NS_LOG_INFO("Setup tracing");
    AsciiTraceHelper ascii;
    lrwpan.EnableAsciiAll(ascii.CreateFileStream ("wnet-IIOT-v1.tr"));
    lrwpan.EnablePcapAll(std::string ("wnet-IIOT-v1"));

    NS_LOG_INFO("Setup animation");
    AnimationInterface anim (animFile);

    NS_LOG_INFO("Setup Simulator");
    Simulator::Stop (Seconds (10.0));
  
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}