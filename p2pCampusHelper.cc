#include "p2pCampusHelper.h"
#include "ns3/core-module.h"
#include <iostream>

using namespace ns3;

PointToPointCampusHelper::PointToPointCampusHelper(uint32_t maxInner, uint32_t maxOuter, PointToPointHelper inner, PointToPointHelper outer, Ptr<UniformRandomVariable> rnd) {
    uint32_t totalNode = maxInner + maxInner * maxOuter + 1;
    // Set up the inner node
    c.Create(totalNode);

    for (int i = 1; i < maxInner + 1; ++i) {
        dev.Add(inner.Install(NodeContainer(c.Get(0), c.Get(i))));
        for (int j = 1; j <= maxOuter; ++j) {
            dev.Add(outer.Install(c.Get(i), c.Get(maxInner + j + (i-1) * maxOuter)));
        }
    }
}

PointToPointCampusHelper::~PointToPointCampusHelper() {

}

Ptr<Node> PointToPointCampusHelper::GetNode(uint32_t i) const {
    return c.Get(i);
}

Ptr<Node> PointToPointCampusHelper::GetHub() const {
    return c.Get(0);
}

uint32_t PointToPointCampusHelper::GetN() const {
    return c.GetN();
}

NodeContainer PointToPointCampusHelper::GetAllNode() const {
    return c;
}

void PointToPointCampusHelper::AssignIpv4Addresses(Ipv4AddressHelper address) {
    for (uint32_t i = 0; i < dev.GetN(); ++i) {
        Ipv4InterfaceContainer ip = address.Assign(dev.Get(i));
    }
}

