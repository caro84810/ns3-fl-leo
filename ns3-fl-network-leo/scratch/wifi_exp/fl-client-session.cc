// fl-client-session.cc

#include "fl-client-session.h"

namespace ns3 {

    ClientSession::ClientSession(int clientID_, double radius_, double theta_) :
            m_client(nullptr), m_radius(radius_), m_theta(theta_), m_clientID(clientID_), m_cycle(0), m_inRound(true),
            m_dropOut(false)
    {
    }

    Ptr<ns3::Socket> ClientSession::GetClient()
    {
        return m_client;
    }

    void ClientSession::SetClient(Ptr<ns3::Socket> client)
    {
        m_client = client;
    }

    bool ClientSession::GetInRound()
    {
        return m_inRound;
    }

    void ClientSession::SetInRound(bool inRound)
    {
        m_inRound = inRound;
    }

    int ClientSession::GetCycle()
    {
        return m_cycle;
    }

    void ClientSession::SetCycle(int cycle)
    {
        m_cycle = cycle;
    }

    void ClientSession::IncrementCycle()
    {
        m_cycle++;
    }

    double ClientSession::GetRadius()
    {
        return m_radius;
    }

    double ClientSession::GetTheta()
    {
        return m_theta;
    }

    int ClientSession::GetClientId()
    {
        return m_clientID;
    }

} // namespace ns3

