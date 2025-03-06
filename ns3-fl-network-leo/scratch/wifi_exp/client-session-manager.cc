// client-session-manager.cc

#include "client-session-manager.h"
#include "fl-client-session.h" // 包含 ClientSession 的定義
#include "ns3/ipv4.h"

namespace ns3 {

    ClientSessionManager::ClientSessionManager(std::map<int, std::shared_ptr<ClientSession>> &inn) :
        m_clientSessionById(inn), m_nInRound(0), m_nInRoundFirstCycleDone(0) {
        
        NS_LOG_UNCOND("初始化ClientSessionManager，總客戶端數: " << m_clientSessionById.size());

        for (auto itr = m_clientSessionById.begin(); itr != m_clientSessionById.end(); itr++) {
            if (itr->second->GetInRound()) {
                NS_LOG_UNCOND("配置客戶端ID: " << itr->first);
                Ptr<Ipv4> ipv4 = itr->second->GetClient()->GetNode()->GetObject<ns3::Ipv4>();
                
                // 遍歷所有非loopback接口
            for (uint32_t i = 1; i < ipv4->GetNInterfaces(); i++) {
                Ipv4Address addr = ipv4->GetAddress(i, 0).GetLocal();
                m_clientSessionByAddress[addr] = itr->second->GetClientId();
                NS_LOG_UNCOND("  映射IP " << addr << " 到客戶端ID " << itr->second->GetClientId());
            }
                m_nInRound++;
            }
        }
        NS_LOG_UNCOND("本輪參與客戶端數: " << m_nInRound);
        // 打印最終映射表
        DumpIPMappings();
    }

    int ClientSessionManager::ResolveToId(ns3::Ipv4Address &address) {
        return m_clientSessionByAddress[address];
    }

    void ClientSessionManager::IncrementCycleCountFromServer(ns3::Ptr<ns3::Socket> socket) {
        NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        auto id = ResolveToIdFromServer(socket);
        if (m_clientSessionById[id]->GetCycle() == 0) {
            m_nInRoundFirstCycleDone++;
        }
        m_clientSessionById[id]->IncrementCycle();
    }

    bool ClientSessionManager::HasAllClientsFinishedFirstCycle() {
        return m_nInRoundFirstCycleDone == m_nInRound;
    }

    int ClientSessionManager::GetRound(ns3::Ptr<ns3::Socket> socket) {
        auto id = ResolveToIdFromServer(socket);
        return m_clientSessionById[id]->GetCycle();
    }

    int ClientSessionManager::ResolveToIdFromServer(ns3::Ptr<ns3::Socket> socket) {
        NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        ns3::Address addr;
        socket->GetPeerName(addr);
        auto temp = ns3::InetSocketAddress::ConvertFrom(addr).GetIpv4();
        return ResolveToId(temp);
    }

    void ClientSessionManager::Close() {
        for (auto itr = m_clientSessionById.begin(); itr != m_clientSessionById.end(); itr++) {
            if (itr->second->GetInRound()) {
                itr->second->GetClient()->Close();
            }
        }
    }

    ns3::Ipv4Address ClientSessionManager::ResolveToAddress(int id) {
        return m_clientSessionById[id]->GetClient()->GetNode()->
                GetObject<ns3::Ipv4>()->GetAddress(1, 0).GetLocal();
    }
    int ClientSessionManager::ResolveToIdFromAddress(ns3::Ipv4Address address) {
        // 檢查是否在m_clientSessionByAddress映射中
        auto it = m_clientSessionByAddress.find(address);
        if (it != m_clientSessionByAddress.end()) {
            return it->second;
        }
        NS_LOG_UNCOND("WARNING: Could not resolve client ID from address " << address);
        return -1;
    }

    int ClientSessionManager::GetRoundFromAddress(ns3::Ipv4Address address) {
        int id = ResolveToIdFromAddress(address);
        if (id >= 0 && m_clientSessionById.find(id) != m_clientSessionById.end()) {
            return m_clientSessionById[id]->GetCycle();
        }
        return -1;
}

    void ClientSessionManager::IncrementCycleCountFromAddress(ns3::Ipv4Address address) {
        int id = ResolveToIdFromAddress(address);
        if (id >= 0 && m_clientSessionById.find(id) != m_clientSessionById.end()) {
            if (m_clientSessionById[id]->GetCycle() == 0) {
                m_nInRoundFirstCycleDone++;
            }
            m_clientSessionById[id]->IncrementCycle();
        }
    }
    
    // 在ClientSessionManager中添加
void ClientSessionManager::DumpIPMappings() {
    NS_LOG_UNCOND("===== IP到客戶端ID映射 =====");
    for (auto const &pair : m_clientSessionByAddress) {
        NS_LOG_UNCOND("IP: " << pair.first << " -> 客戶端ID: " << pair.second);
    }
}
} // namespace ns3

