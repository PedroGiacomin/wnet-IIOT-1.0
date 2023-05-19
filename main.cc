// Network topology
//
//       n0    n1   n2   n3       n21
//       |     |    |    |        |
//       ================= ... ===
//              LAN 10.0.0.x
//
// - Servidor em n0, clientes em n1 ... n21
// - Todos os clientes pingam no servidor e em todos os outros clientes, um de cada vez

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-spectrum-value-helper.h"
#include "ns3/spectrum-value.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/mobility-module.h"

#include "ns3/netanim-module.h"
#include "ns3/v4ping-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("UdpClientsServerIOT");

int main (int argc, char *argv[]){
    uint16_t numNodes = 5;

    //Permite mudar a quantidade de nodes clientes na propria chamada da aplicacao
    CommandLine cmd;
	cmd.AddValue ("numNodes", "Number of node devices", numNodes);
	cmd.Parse (argc,argv);

    NS_LOG_INFO ("Set up logging.");
    //LogComponentEnable ("wnet-IIOT-1.0", LOG_LEVEL_ALL);

    NS_LOG_INFO ("Create nodes.");
    NodeContainer nodes, apNode; //apNode salva o 'servidor'
    nodes.Create(numNodes + 1);
    apNode.Create(1);

    NodeContainer allNodes(nodes, apNode);

    NS_LOG_INFO("Install internet protocols stack.");
    InternetStackHelper internet;
    internet.SetIpv4StackInstall(false);
	internet.SetIpv6StackInstall(true);
	internet.Install(allNodes);

    //Instala o enlace IEEE 802.15.4 - Low Rate WPAN
    NS_LOG_INFO("Create and install Lr-WPAN channel.");
    LrWpanHelper lrwpan(false);
	NetDeviceContainer netDevices = lrwpan.Install(allNodes); 
	lrwpan.AssociateToPan(netDevices, 0); //Associa os nodes dessa rede na PAN de ID = 0

    NS_LOG_INFO("Create and install 6LowPAN."); 
    SixLowPanHelper sixlowpan;  //6LoWPAN permite o Lrwpan usar IPv6
    NetDeviceContainer sixNetDevices = sixlowpan.Install(netDevices);

    NS_LOG_INFO("Create and install IPv6 adresses.");
    Ipv6AddressHelper ipv6;
  	ipv6.SetBase (Ipv6Address ("2020:1::"), Ipv6Prefix (64));
  	Ipv6InterfaceContainer interface = ipv6.Assign(sixNetDevices);

    NS_LOG_INFO("Set up mobility.");
    MobilityHelper mobility, apMobilityHelper;
    
    //Posiciona nodes em um grid, [nodesPerLine] nodes por linha e [squarePerNode] de distancia entre nodes adjacentes (nas duas dimensoes) 
    // --------- DUVIDA ------------//
    double squarePerNode = ceil(sqrt(40000/numNodes));
	double nodesPerLine = ceil(200/squarePerNode);
    // --------- DUVIDA ------------//
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",    
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(squarePerNode),
        "DeltaY", DoubleValue(squarePerNode),
        "GridWidth", UintegerValue(nodesPerLine),
        "LayoutType", StringValue("RowFirst")
    );
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	mobility.Install(nodes);

    //Posiciona o AP no ponto 100,100
    apMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobilityHelper.Install(apNode);
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
	apMobility->SetPosition(Vector(100, 100, 0.0));

    // --------- DUVIDA ----------//
    NS_LOG_INFO("Set AP range.");
    Ptr<Node> ap = apNode.Get(0);
	Ptr<LrWpanNetDevice> apnetdev = DynamicCast<LrWpanNetDevice>(ap->GetDevice(1)); //Faz cast de NetDevice para LrWpanNetDevice, mas pq pega o 1?
	auto apphy = apnetdev->GetPhy();
	LrWpanSpectrumValueHelper svh;
	Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity (9, 11); //Range of 200m according to lr-wpan-error-distance-plot
	apphy->SetTxPowerSpectralDensity(psd);
    
    NS_LOG_INFO("Set nodes range.");
    for (size_t i = 0; i < nodes.GetN(); i++){
        Ptr<LrWpanNetDevice> nodenetdev = DynamicCast<LrWpanNetDevice>(nodes.Get(i)->GetDevice(1));
		auto phy = nodenetdev->GetPhy();
		LrWpanSpectrumValueHelper svh;
		Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity(-10, 11); //Range of 50m according to lr-wpan-error-distance-plot
		phy->SetTxPowerSpectralDensity(psd);
    }
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

    // --- EXECUCAO --- //
    NS_LOG_INFO("Run simulation.");
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

    return 0;
}