// client-session-manager.h

#ifndef CLIENTSESSIONMANAGER_H
#define CLIENTSESSIONMANAGER_H

#include <map>
#include <memory>
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"

namespace ns3 {
    class ClientSession; // 前置宣告

    /**
     * \ingroup fl-client-session-manager
     * \brief Manages the client session
     */
    class ClientSessionManager {
    public:
    	 // 通過IP地址解析客戶端ID
    	int ResolveToIdFromAddress(Ipv4Address address);
    
    	// 通過IP地址獲取輪數
   	 int GetRoundFromAddress(Ipv4Address address);
    
   	 // 通過IP地址增加循環計數
   	 void IncrementCycleCountFromAddress(Ipv4Address address);
        /**
         * \brief Construct client session manager
         * \param inn    map of < client ids, client sessions >
         */
        ClientSessionManager(std::map<int, std::shared_ptr<ClientSession>> &inn);

        /**
         * \brief Get client id from client address
         * \param address  Client address
         * \return  Client id
         */
        int ResolveToId(ns3::Ipv4Address &address);

        /**
         * \brief Increment cycle count (async) from server
         * \param socket  Client socket
         */
        void IncrementCycleCountFromServer(ns3::Ptr<ns3::Socket> socket);

        /**
         * \brief Returns if all of the clients finished 1 cycle (async)
         * \return  True if all clients finished a cycle
         */
        bool HasAllClientsFinishedFirstCycle();

        /**
         * \brief Get cycle of client in async round
         * \param socket  Client socket to check round for
         */
        int GetRound(ns3::Ptr<ns3::Socket> socket);

        /**
         * \brief Get client id from client socket
         * \param socket  Client socket
         * \return  Client id
         */
        int ResolveToIdFromServer(ns3::Ptr<ns3::Socket> socket);

        /**
         * \brief Close client socket
         */
        void Close();

        /**
         * \brief Get client address from client id
         * \param id  Client id
         * \return  Client address
         */
        ns3::Ipv4Address ResolveToAddress(int id);
        void DumpIPMappings();

    private:
        std::map<ns3::Ipv4Address, int> m_clientSessionByAddress;                 //!< maps Client Address to Client id
        std::map<int, std::shared_ptr<ClientSession>> &m_clientSessionById;      //!< maps client id to client session
        int m_nInRound;                                                           //!< number of clients in round
        int m_nInRoundFirstCycleDone;                                             //!< number of clients with first cycle done
    };

} // namespace ns3

#endif // CLIENTSESSIONMANAGER_H

