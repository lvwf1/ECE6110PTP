// @Author: Mengya Gao, WeiFeng Lyu
#include <iostream>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <iomanip>
#include <sys/time.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mpi-interface.h"
#include "p2pCampusHelper.h"

using namespace ns3;

uint32_t port = 8888;



uint32_t sequentialCnt;
bool isInfected[4][25];
NodeContainer infectedNode;

void checkAndSetInfected(std::string ip, Ptr<Node> node) {
    for (int i = 0; i < infectedNode.GetN(); ++i) {
        if (infectedNode.Get(i) == node) {
            return;
        }
    }

    infectedNode.Add(node);
    if (infectedNode.GetN() == 72) {
        Simulator::Stop();
    }
}

static Ipv4Address chooseIp(std::string scanPattern, uint32_t subnetId) {
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();

    double i, j;
    if(scanPattern == "Uniform")
    {
        i = uv->GetValue(1.0, 4.0);
        j = uv->GetValue(1.0, 48.0);
    } else if (scanPattern == "Local") {
        i = (double) subnetId;
        if(uv->GetValue(0.0, 1.0) > 0.65){
            while((uint32_t) i == subnetId){
                i = uv->GetValue(1.0, 7.0);
            }
        }
        j = uv->GetValue(1.0, 48.0);
    } else {
        i = sequentialCnt/48 + 1;
        j = sequentialCnt%48 + 1;
        sequentialCnt++;
    }

    char buff[13];
    std::sprintf(buff, "10.1.%d.%d", (uint32_t)i, (uint32_t)j);
    ns3::Ipv4Address address = ns3::Ipv4Address(buff);
    return address;
}

void infectNodes(uint32_t scanRate, std::string scanPattern) {
    for (int i = 0; i < infectedNode.GetN(); ++i) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> client = Socket::CreateSocket(infectedNode.Get(i), tid);
        Ipv4Address randomIp = chooseIp(scanPattern, 1);
        InetSocketAddress serverAddr = InetSocketAddress(randomIp, port);
        client->Connect(serverAddr);
        client->Send(Create<Packet>(500));
        client->Close();
    }
    ns3::Simulator::Schedule(ns3::Seconds(scanRate),&infectNodes, scanRate, scanPattern);
}

static void recvCallback(Ptr<Socket> sock) {
    Ptr<Node> node = sock->GetNode();
    checkAndSetInfected("10.1.1.1", node);
}

typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec * 1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1) - TIMER_SECONDS (_t2))

int main(int argc, char *argv[]) {

    TIMER_TYPE t0, t1, t2;
    TIMER_NOW (t0);

    uint32_t scanRate = 5;
    uint32_t nInner = 8;
    uint32_t nOuter = 2;
    std::string backboneDelay = "10ms";
    std::string scanPattern = "Uniform";
    std::string syncType = "Yawns";
    sequentialCnt = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 25; ++j) {
            isInfected[i][j] = false;
        }
    }

    CommandLine cmd;
    cmd.AddValue("ScanRate", "Rate to generate worm traffic (5, 10, 20) ms", scanRate);
    cmd.AddValue("ScanPattern", "Scanning pattern (Uniform, Local, Sequential)", scanPattern);
    cmd.AddValue("BackboneDelay", "ms delay for backbone links (lookahead)", backboneDelay);
    cmd.AddValue("SyncType", "Conservative algorithm", syncType);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed(1);

    if (syncType == "Yawns") {
        GlobalValue::Bind ("SimulatorImplementationType",
                           StringValue ("ns3::DistributedSimulatorImpl"));
    } else {
        GlobalValue::Bind ("SimulatorImplementationType",
                           StringValue ("ns3::NullMessageSimulatorImpl"));
    }

    MpiInterface::Enable(&argc, &argv);
    // Get rank and total number of CPUs.
    uint32_t systemId = MpiInterface::GetSystemId ();
    uint32_t systemCount = MpiInterface::GetSize ();


    Ptr<UniformRandomVariable> rn = CreateObject<UniformRandomVariable> ();
    PointToPointHelper inner;
    PointToPointHelper outer;
    inner.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    inner.SetChannelAttribute("Delay", StringValue("5ms"));
    outer.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    outer.SetChannelAttribute("Delay", StringValue("8ms"));

    PointToPointHelper hub2hub_10ms;
    hub2hub_10ms.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    hub2hub_10ms.SetChannelAttribute("Delay", StringValue(backboneDelay));

    PointToPointHelper hub2hub_100ms;
    hub2hub_100ms.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    hub2hub_100ms.SetChannelAttribute("Delay", StringValue(backboneDelay));

    PointToPointHelper hub2hub_500ms;
    hub2hub_500ms.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    hub2hub_500ms.SetChannelAttribute("Delay", StringValue(backboneDelay));


    PointToPointCampusHelper topo1(nInner, nOuter, inner, outer, rn);
    PointToPointCampusHelper topo2(nInner, nOuter, inner, outer, rn);
    PointToPointCampusHelper topo3(nInner, nOuter, inner, outer, rn);
    PointToPointCampusHelper topo4(nInner, nOuter, inner, outer, rn);

    NetDeviceContainer hubDev1 = hub2hub_10ms.Install(topo1.GetHub(), topo2.GetHub());
    NetDeviceContainer hubDev2 = hub2hub_10ms.Install(topo2.GetHub(), topo3.GetHub());
    NetDeviceContainer hubDev3 = hub2hub_10ms.Install(topo3.GetHub(), topo4.GetHub());

    // Enable Nix-Vector routing
    Ipv4NixVectorHelper nixRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(nixRouting);
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    topo1.AssignIpv4Addresses(address);

    address.SetBase("10.1.2.0", "255.255.255.0");
    topo2.AssignIpv4Addresses(address);

    address.SetBase("10.1.3.0", "255.255.255.0");
    topo3.AssignIpv4Addresses(address);

    address.SetBase("10.1.4.0", "255.255.255.0");
    topo4.AssignIpv4Addresses(address);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIp1 = address.Assign(hubDev1);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIp2 = address.Assign(hubDev2);

    address.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIp3 = address.Assign(hubDev3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    if(systemId == 0%systemCount) {
        for (int i = 0; i < topo1.GetN(); ++i) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> server = Socket::CreateSocket(topo1.GetNode(i), tid);
            InetSocketAddress addr = InetSocketAddress(Ipv4Address::GetAny(), port);
            server->Bind(addr);
            server->SetRecvCallback(MakeCallback(&recvCallback));
        }
    }

    if (systemId == 1 % systemCount) {
        for (int i = 0; i < topo2.GetN(); ++i) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> server = Socket::CreateSocket(topo2.GetNode(i), tid);
            InetSocketAddress addr = InetSocketAddress(Ipv4Address::GetAny(), port);
            server->Bind(addr);
            server->SetRecvCallback(MakeCallback(&recvCallback));
        }
    }

    if (systemId == 2 % systemCount) {
        for (int i = 0; i < topo3.GetN(); ++i) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> server = Socket::CreateSocket(topo3.GetNode(i), tid);
            InetSocketAddress addr = InetSocketAddress(Ipv4Address::GetAny(), port);
            server->Bind(addr);
            server->SetRecvCallback(MakeCallback(&recvCallback));
        }
    }

    if (systemId == 3 % systemCount) {
        for (int i = 0; i < topo4.GetN(); ++i) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> server = Socket::CreateSocket(topo4.GetNode(i), tid);
            InetSocketAddress addr = InetSocketAddress(Ipv4Address::GetAny(), port);
            server->Bind(addr);
            server->SetRecvCallback(MakeCallback(&recvCallback));
        }
    }

    infectedNode.Add(topo1.GetNode(2));
    isInfected[0][2] = true;


    ns3::Simulator::Schedule(ns3::Seconds(scanRate),&infectNodes, scanRate, scanPattern);

//    Simulator::Stop(Seconds(50.0));
    TIMER_NOW(t1)
    Simulator::Run();
    TIMER_NOW(t2)
    Simulator::Destroy();

    MpiInterface::Disable ();
    double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
    std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
    std::cout << "Simulator init time: " << d1 << std::endl;
    std::cout << "Simulator run time: " << d2 << std::endl;
    std::cout << "Total elapsed time: " << d1 + d2 << std::endl;

    return 0;
}
