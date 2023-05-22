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
    uint16_t numNodes = 1;
    std::string animFile = "wnetIIOTv1-animation.xml";

    LogComponentEnable ("WnetIIOTv1", LOG_LEVEL_INFO);

    NS_LOG_INFO ("Create nodes.");
    NodeContainer nodes, apNode; //apNode salva o 'servidor'
    nodes.Create(numNodes);
    apNode.Create(1);
    NodeContainer allNodes(nodes, apNode); 

    NS_LOG_INFO("Set up mobility.");
    double distNodes = 10;
	double nodesPerLine = 5;
    double apPosX = 100, apPosY = 100;

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
  	ipv6.SetBase (Ipv6Address ("2020:1::"), Ipv6Prefix (64));
  	Ipv6InterfaceContainer subnet1 = ipv6.Assign(netDevices);  //Cria rede com IP 2020:1:0:0:x:x:x:x

    NS_LOG_INFO("Setup and install ping applications.");
    uint32_t packetSize = 512;
    uint32_t maxPacketCount = 1;
    Time interPacketInterval = Seconds (1.);
    Ping6Helper ping6_N2N;
    Ping6Helper ping6_N2AP;

    //---- CHECKPOINT ----//
    // Ainda não entendi muito bem como eh esse enderecamento com dois argumentos
    // abaixo, nem como os endereços aparecem no Wireshark. Não foi setado range 
    // para os nodes.
    //ping node0 -> node1
    ping6_N2N.SetLocal(subnet1.GetAddress(0, 0));
    ping6_N2N.SetRemote(subnet1.GetAddress(1, 0));
    ping6_N2N.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    ping6_N2N.SetAttribute ("Interval", TimeValue (interPacketInterval));
    ping6_N2N.SetAttribute ("PacketSize", UintegerValue (packetSize));

    //ping node0 -> AP
    // ping6_N2AP.SetLocal(subnet1.GetAddress(0, 1));
    // ping6_N2AP.SetRemote(subnet1.GetAddress(2, 1));
    // ping6_N2AP.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    // ping6_N2AP.SetAttribute ("Interval", TimeValue (interPacketInterval));
    // ping6_N2AP.SetAttribute ("PacketSize", UintegerValue (packetSize));

    //ApplicationContainer apps_N2AP = ping6_N2AP.Install(nodes.Get(0));
    ApplicationContainer apps_N2N = ping6_N2N.Install(nodes.Get(0));

    // apps_N2AP.Start (Seconds (1.0));
    // apps_N2AP.Stop (Seconds (10.0));
    apps_N2N.Start (Seconds (2.0));
    apps_N2N.Stop (Seconds (10.0));

    NS_LOG_INFO("Setup tracing");
    AsciiTraceHelper ascii;
    lrwpan.EnableAsciiAll (ascii.CreateFileStream ("wnet-IIOT-v1.tr"));
    lrwpan.EnablePcapAll (std::string ("wnet-IIOT-v1"), true);

    NS_LOG_INFO("Setup animation");
    AnimationInterface anim (animFile);

    auto apAddress = apNode.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();
    NS_LOG_INFO("AP: " << apAddress);
    for (size_t i = 0; i < nodes.GetN(); i++){
		Ptr<MobilityModel> deviceMobility = nodes.Get(i)->GetObject<MobilityModel>();
		double distance = deviceMobility->GetDistanceFrom(apMobility);

		Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
        Ipv6InterfaceAddress iaddr = ipv6->GetAddress(1, 0);
        Ipv6Address ipAddr = iaddr.GetAddress();

		NS_LOG_INFO("N: IP " << ipAddr << " D " << distance);
	}

    NS_LOG_INFO("Setup Simulator");
    Simulator::Stop (Seconds (10));
  
    Simulator::Run ();
    Simulator::Destroy ();

    // --------- PARTE DO RANGE ----------//
    // NS_LOG_INFO("Set AP range.");
    // Ptr<Node> ap = apNode.Get(0);
	// Ptr<LrWpanNetDevice> apnetdev = DynamicCast<LrWpanNetDevice>(ap->GetDevice(1)); //Faz cast de NetDevice para LrWpanNetDevice, mas pq pega o 1?
	// auto apphy = apnetdev->GetPhy();
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

    // --- APLICACOES --- //
    // So existem aplicacoes ping nos clientes, no servidor nao ha aplicacao
    // Cada client tem numNodes-1 aplicacoes ping, cada uma com um endereco de destino (so nao tem a do proprio endereco)
    // Cada aplicacao eh uma posicao do vetor pingApps
    // NS_LOG_INFO("Set applications.");
    // std::vector<ApplicationContainer> pingApps(numClients);
    // for(uint16_t i = 0; i < numClients; ++i){
    //     for(uint16_t j = 0; j < (numClients + 1); ++j){
    //         if(j != i){
    //             V4PingHelper pingHelperAux(interface.GetAddress(j)); //pinga inclusive no servidor
    //             pingHelperAux.SetAttribute ("Verbose", BooleanValue (false));
    //             pingHelperAux.SetAttribute ("Interval", TimeValue (Seconds(numClients)));
    //             pingHelperAux.SetAttribute ("Size", UintegerValue (16));
    //             pingApps[i].Add(pingHelperAux.Install(allNodes.Get(i))); // Instala ping(dest: j) no cliente i
    //         }
    //     }
    //}

    // Os nodes comecam a pingar com 1 segundo de diferenca
    // pingApps[i] == aplicacao do cliente [i]
    // for(uint16_t i = 0; i < numClients; ++i){
    //     uint16_t appStart = 1.0 + 1.0*i;
    //     pingApps[i].Start(Seconds(appStart));
    //     pingApps[i].Stop(Seconds(appStart + (numClients) * 2 + 1.0)); //Termina apos enviar 3 pacotes
    // }
    
    // TRACING
    // AsciiTraceHelper ascii;
    // csma.EnableAsciiAll (ascii.CreateFileStream ("clients-server-IOT.tr"));
    // csma.EnablePcapAll ("clients-server-IOT.tr", false);

    // --- NETANIM --- //
    // NS_LOG_INFO("Set animation.");
    // AnimationInterface anim ("clients-server-IOT-anim.xml");

    // uint32_t X0 = 20, Y0 = 20, x = 0, y = 0;
    // for(uint32_t i= 0; i<numClients; ++i){
    //     x = X0 + (10 * (i%5));
    //     y = Y0 + (10 * int(i/5));
    //     anim.SetConstantPosition(allNodes.Get(i), x, y);
    // }
    // anim.SetConstantPosition(allNodes.Get(numClients), (X0 + 20), (X0 - 20)); //server = node[numClients]
    return 0;
}