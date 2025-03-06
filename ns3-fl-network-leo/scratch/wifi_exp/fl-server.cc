#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "fl-server.h"
#include "ns3/boolean.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/inet-socket-address.h"
#include "fl-sim-interface.h"
#include "client-session-manager.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("Server");

    NS_OBJECT_ENSURE_REGISTERED (Server);

    TypeId
    Server::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::Server")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<Server>()

                .AddAttribute("DataRate", "The data rate in on state.",
                              DataRateValue(DataRate("1b/s")),
                              MakeDataRateAccessor(&Server::m_dataRate),
                              MakeDataRateChecker())

                .AddAttribute("Local",
                              "The Address on which to Bind the rx socket.",
                              AddressValue(),
                              MakeAddressAccessor(&Server::m_local),
                              MakeAddressChecker())
                .AddAttribute("Protocol",
                              "The type id of the protocol to use for the rx socket.",
                              TypeIdValue(UdpSocketFactory::GetTypeId()),
                              MakeTypeIdAccessor(&Server::m_tid),
                              MakeTypeIdChecker())

                .AddAttribute("MaxPacketSize",
                              "MaxPacketSize to send to client",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&Server::m_packetSize),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("Async",
                              "Run as async client",
                              TypeId::ATTR_SGC,
                              BooleanValue(false),
                              MakeBooleanAccessor(&Server::m_bAsync),
                              MakeBooleanChecker())
                .AddAttribute("BytesModel",
                              "Number of bytes in model",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&Server::m_bytesModel),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("TimeOffset",
                              "Time offset to add to simulation time reported",
                              TypeId::ATTR_SGC,
                              TimeValue(),
                              MakeTimeAccessor(&Server::m_timeOffset),
                              MakeTimeChecker());


        return tid;
    }

    Server::Server() : m_packetSize(0), m_sendEvent(), m_bytesModel(0), m_bAsync(false), m_fLSimProvider(nullptr) {
        m_socket = 0;
    }

    Server::~Server() {
        NS_LOG_FUNCTION(this);
    }

    std::map <Ptr<Socket>, std::shared_ptr<Server::ClientSessionData>>
    Server::GetAcceptedSockets(void) const {
        NS_LOG_FUNCTION(this);
        return m_socketList;
    }

    void Server::DoDispose(void) {
        NS_LOG_FUNCTION(this);
        m_socket = 0;
        m_socketList.clear();

        // chain up
        Application::DoDispose();
    }

// Application Methods
    void Server::StartApplication()    // Called at time specified by Start
    {
        NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        NS_LOG_FUNCTION(this);
        // 創建socket如果沒有
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), m_tid);
            
            // 檢查是否是UDP
            if (m_tid == UdpSocketFactory::GetTypeId()) {
                NS_LOG_UNCOND("服務器準備綁定UDP socket到端口80");
                if (m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 80)) == -1) {
                        NS_FATAL_ERROR("Failed to bind UDP socket");
                }
                NS_LOG_UNCOND("服務器成功綁定到所有接口:80");
                // 為UDP設置接收回調
                m_socket->SetRecvCallback(MakeCallback(&Server::ReceivedDataCallback, this));
                 NS_LOG_UNCOND("服務器接收回調已設置");
            } else {
                NS_LOG_UNCOND("Server starting with TCP socket");
                // TCP需要Listen和Accept
                if (m_socket->Bind(m_local) == -1) {
                    NS_FATAL_ERROR("Failed to bind TCP socket");
                }
                if (m_socket->Listen() == -1) {
                    NS_FATAL_ERROR("Failed to listen socket");
                }
                
                m_socket->SetRecvCallback(MakeCallback(&Server::ReceivedDataCallback, this));
                m_socket->SetAcceptCallback(
                        MakeCallback(&Server::ConnectionRequestCallback, this),
                        MakeCallback(&Server::NewConnectionCreatedCallback, this));
                m_socket->SetCloseCallbacks(
                        MakeCallback(&Server::HandlePeerClose, this),
                        MakeCallback(&Server::HandlePeerError, this));
            }
        }
        m_completedUdpClients = 0;
        m_totalSelectedUdpClients = 0;  // 將在設置客戶端時更新

    }

    void Server::StopApplication()     // Called at time specified by Stop
    {
        NS_LOG_FUNCTION(this);
        NS_LOG_UNCOND("Stopping Application");

        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }

        //Close all connections
        for (auto const &itr: m_socketList) {
            itr.first->Close();
            itr.first->SetRecvCallback(MakeNullCallback < void, Ptr < Socket > > ());
        }

        if (m_socket) {
            m_socket->Close();
            m_socket->SetRecvCallback(MakeNullCallback < void, Ptr < Socket > > ());
        }

    }

    void Server::ReceivedDataCallback(Ptr <Socket> socket) {
        NS_LOG_UNCOND("===== 服務器接收回調被觸發 =====");
        NS_LOG_FUNCTION(this << socket);
        
        Ptr<Packet> packet;
        Address from;
        Address localAddress;
        
        bool isUdp = (socket->GetSocketType() == Socket::NS3_SOCK_DGRAM);
        
        while ((packet = socket->RecvFrom(from))) {
         NS_LOG_UNCOND("服務器收到數據包: 大小=" << packet->GetSize() 
                    << "bytes, 來源=" << InetSocketAddress::ConvertFrom(from).GetIpv4()
                    << ":" << InetSocketAddress::ConvertFrom(from).GetPort());
            if (packet->GetSize() == 0) { //EOF
                break;
            }
            if (packet->GetSize() == 8) {
                uint8_t buffer[8];
                packet->CopyData(buffer, 8);
                if (buffer[0] == 0xAC && buffer[1] == 0xAC) {
                    NS_LOG_UNCOND("收到客戶端ACK確認");
                    // 處理確認邏輯...
                    return;
                }
            }
            
            NS_LOG_UNCOND("Server received " << packet->GetSize() << " bytes from " 
                        << InetSocketAddress::ConvertFrom(from).GetIpv4());
            
            std::shared_ptr<ClientSessionData> clientSession;
            
            // 對UDP客戶端特殊處理
            if (isUdp) {                  
                // 查找或創建此客戶端的會話數據
                auto itr = m_udpClientMap.find(InetSocketAddress::ConvertFrom(from).GetIpv4());
                if (itr == m_udpClientMap.end()) {
                    NS_LOG_UNCOND("New UDP client detected: " << InetSocketAddress::ConvertFrom(from).GetIpv4());
                    clientSession = std::make_shared<ClientSessionData>();
                    clientSession->m_address = from;
                    clientSession->m_bytesReceived = 0;  // 初始化為0
                    clientSession->m_bytesModelToReceive = m_bytesModel;
                    clientSession->m_bytesModelToSend = m_bytesModel;
        
                    m_udpClientMap[InetSocketAddress::ConvertFrom(from).GetIpv4()] = clientSession;
                    
                    // 為新UDP客戶端初始化數據
                    if (m_clientSessionManager) {
                        int clientId = m_clientSessionManager->ResolveToIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
                        NS_LOG_UNCOND("Mapped UDP client to ID: " << clientId);
                        
                        // 初始化發送模型過程
                        clientSession->m_bytesModelToReceive = m_bytesModel;
                        clientSession->m_bytesModelToSend = m_bytesModel;
                        clientSession->m_timeBeginSendingModelFromClient = Simulator::Now();
                        
                        // 立即開始向此客戶端發送數據
                        SendModelUDP(socket, from);
                    }
                } else {
                    clientSession = itr->second;
                }       
                                        
               NS_LOG_UNCOND("服務器收到數據包: 大小=" << packet->GetSize() 
             << "bytes, 來源=" << InetSocketAddress::ConvertFrom(from).GetIpv4());
            } else {
                // TCP客戶端處理
                auto itr = m_socketList.find(socket);
                if (itr == m_socketList.end()) {
                    NS_LOG_UNCOND("Received data from unknown TCP client");
                    return;
                }
                clientSession = itr->second;
            }
            
            // 共同的數據處理邏輯
            if (clientSession->m_bytesReceived % m_bytesModel == 0) {
                clientSession->m_timeBeginReceivingModelFromClient = Simulator::Now();
            }

            clientSession->m_bytesReceived += packet->GetSize();
            clientSession->m_bytesModelToReceive -= packet->GetSize();
            
            NS_LOG_UNCOND("Client session received " << clientSession->m_bytesReceived 
                        << " bytes, " << clientSession->m_bytesModelToReceive << " bytes remaining");

            if (clientSession->m_bytesModelToReceive == 0) {
                NS_LOG_UNCOND("Client " << (isUdp ? "UDP" : "TCP") << " has completed receiving model data");

            clientSession->m_timeEndReceivingModelFromClient = Simulator::Now();

            auto endUplink = clientSession->m_timeEndReceivingModelFromClient.GetSeconds() + m_timeOffset.GetSeconds();
            auto beginUplink = clientSession->m_timeBeginReceivingModelFromClient.GetSeconds() + m_timeOffset.GetSeconds();
            auto beginDownlink = clientSession->m_timeBeginSendingModelFromClient.GetSeconds() + m_timeOffset.GetSeconds();
            auto endDownlink = clientSession->m_timeEndSendingModelFromClient.GetSeconds() + m_timeOffset.GetSeconds();

            auto energy = FLEnergy();
            energy.SetDeviceType("400");
            energy.SetLearningModel("CIFAR-10");
            energy.SetEpochs(5.0);
            double compEnergy = energy.CalcComputationalEnergy(beginUplink-endDownlink);
            double tranEnergy = energy.CalcTransmissionEnergy(endUplink-beginUplink);
            NS_LOG_UNCOND(energy.GetA());
            
            // 為UDP和TCP都記錄能量消耗
            if (isUdp) {
                auto clientId = m_clientSessionManager->ResolveToIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
                fprintf(m_fp, "%i,%u,%f,%f,%f,%f,%f,%f\n",
                        m_round,
                        clientId,
                        beginUplink, endUplink,
                        beginDownlink, endDownlink,
                        compEnergy, tranEnergy
                );
            } else {
                fprintf(m_fp, "%i,%u,%f,%f,%f,%f,%f,%f\n",
                        m_round,
                        m_clientSessionManager->ResolveToIdFromServer(socket),
                        beginUplink, endUplink,
                        beginDownlink, endDownlink,
                        compEnergy, tranEnergy
                );
            }
            fflush(m_fp);

            if (m_bAsync) {
                NS_LOG_UNCOND("Entering async branch in ReceivedDataCallback");
                if (m_fLSimProvider) {
                    NS_LOG_UNCOND("m_fLSimProvider is valid");
                    FLSimProvider::AsyncMessage message;
                    
                    // 根據協議類型獲取客戶端ID
                    if (isUdp) {
                        message.id = m_clientSessionManager->ResolveToIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
                    } else {
                        message.id = m_clientSessionManager->ResolveToIdFromServer(socket);
                    }
                    
                    message.endTime = endUplink;
                    message.startTime = beginDownlink;
                    message.throughput = clientSession->m_bytesReceived * 8.0 / 1000.0 /
                                        ((endUplink - beginUplink));

                    NS_LOG_UNCOND("Sending async message for client " << message.id 
                                << " with endTime=" << message.endTime 
                                << ", startTime=" << message.startTime 
                                << ", throughput=" << message.throughput);
                    
                    m_fLSimProvider->send(&message);
                    NS_LOG_UNCOND("Message sent successfully");

                    // 顯示詳細日誌
                    Ipv4Address clientAddr;
                    if (isUdp) {
                        clientAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
                    } else {
                        socket->GetPeerName(from);
                        clientAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
                    }
                    
                    // 獲取正確的輪次信息
                    int currentRound;
                    if (isUdp) {
                        currentRound = m_clientSessionManager->GetRoundFromAddress(clientAddr);
                    } else {
                        currentRound = m_clientSessionManager->GetRound(socket);
                    }
                    
                    NS_LOG_UNCOND(
                            "[SERVER]  " << clientAddr << " -> 10.1.1.1" << std::endl << 
                            "  Round " << currentRound << std::endl << 
                            "  Begin downlink=" << beginDownlink << std::endl << 
                            "  Begin uplink=" << beginUplink << std::endl << 
                            "  End uplink=" << endUplink << std::endl << 
                            "  Round time=" << (endUplink - beginDownlink) << std::endl << 
                            "  UplinkDifference=" << (endUplink - beginUplink) << std::endl);
                } else {
                    NS_LOG_UNCOND("ERROR: m_fLSimProvider is NULL");
                }

                // 對UDP客戶端特殊處理循環
                if (isUdp) {
                    // 增加循環計數
                    m_clientSessionManager->IncrementCycleCountFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
                    
                    if (m_clientSessionManager->HasAllClientsFinishedFirstCycle() ||
                        (m_clientSessionManager->GetRoundFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4()) >= 3)) {
                        m_clientSessionManager->Close();
                        if (m_fLSimProvider) {
                           NS_LOG_UNCOND("Calling FLSimProvider::end() to signal simulation end to Python");
                           // 添加日誌以確認結果
                           m_fLSimProvider->end();
                           NS_LOG_UNCOND("end() called successfully");

                        }else {
                           NS_LOG_UNCOND("ERROR: m_fLSimProvider is NULL when trying to end simulation");
                        }

                        m_timeOffset = Simulator::Now() + Time(m_timeOffset.GetSeconds());
                        NS_LOG_UNCOND("STOPPING_SIMULATION" << std::endl << std::endl);
                        Simulator::Stop();
                    } else {
                        // 繼續向此UDP客戶端發送下一個模型
                        clientSession->m_bytesModelToReceive = m_bytesModel;
                        clientSession->m_bytesModelToSend = m_bytesModel;
                        clientSession->m_timeBeginSendingModelFromClient = Simulator::Now();
                        SendModelUDP(socket, from);
                    }
                } else {
                    // TCP客戶端處理 - 使用原始代碼
                    m_clientSessionManager->IncrementCycleCountFromServer(socket);

                    if (m_clientSessionManager->HasAllClientsFinishedFirstCycle() ||
                        (m_clientSessionManager->GetRound(socket) == 3)) {
                        m_clientSessionManager->Close();
                        if (m_fLSimProvider) {
                            m_fLSimProvider->end();
                        }

                        m_timeOffset = Simulator::Now() + Time(m_timeOffset.GetSeconds());
                        NS_LOG_UNCOND("STOPPING_SIMULATION" << std::endl << std::endl);
                        Simulator::Stop();
                    } else {
                        StartSendingModel(socket);
                    }
                }
            }
        }

        socket->GetSockName(localAddress);
    }
}

    // 新增UDP模型發送方法
    void Server::SendModelUDP(Ptr<Socket> socket, Address clientAddress) {
        NS_LOG_FUNCTION(this << socket);
    
    auto clientIp = InetSocketAddress::ConvertFrom(clientAddress).GetIpv4();
    auto itr = m_udpClientMap.find(clientIp);
    
    if (itr == m_udpClientMap.end() || itr->second->m_bytesModelToSend == 0) {
        NS_LOG_UNCOND("No model to send to UDP client " << clientIp);
        return;
    }

    // 計算要發送的字節數
    auto bytes = std::min(itr->second->m_bytesModelToSend, m_packetSize);
    
    //NS_LOG_UNCOND("Sending " << bytes << " bytes to UDP client " << clientIp);
    
    // 發送數據包
    auto bytesSent = socket->SendTo(Create<Packet>(bytes), 0, clientAddress);
    
    if (bytesSent == -1) {
        NS_LOG_UNCOND("Failed to send data to UDP client " << clientIp << ", error: " << socket->GetErrno());
        // 嘗試處理錯誤
        if (socket->GetErrno() == Socket::ERROR_NOTERROR) {
            // 可能只是暫時無法發送，稍後重試
            Simulator::Schedule(MicroSeconds(100), &Server::SendModelUDP, this, socket, clientAddress);
        }
        return;
    }
    
    itr->second->m_bytesSent += bytesSent;
    itr->second->m_bytesModelToSend -= bytesSent;
    
    if (itr->second->m_bytesModelToSend) {
        // 計算下一次發送的時間
        Time nextTime(Seconds((bytes * 8) / static_cast<double>(m_dataRate.GetBitRate())));
        
        // 安排下一次發送
        Simulator::Schedule(nextTime, &Server::SendModelUDP, this, socket, clientAddress);
    } else {
        // 完成了整個模型的發送
        itr->second->m_timeEndSendingModelFromClient = Simulator::Now();
        NS_LOG_UNCOND("Completed sending model to UDP client " << clientIp);
    }
    }

    void Server::HandlePeerClose(Ptr <Socket> socket) {
        NS_LOG_FUNCTION(this << socket);
    }

    void Server::HandlePeerError(Ptr <Socket> socket) {
        NS_LOG_FUNCTION(this << socket);
    }

    bool Server::ConnectionRequestCallback(Ptr <Socket> socket, const Address &address) {
        NS_LOG_FUNCTION(this << socket << address);
        return true;
    }


    void Server::NewConnectionCreatedCallback(Ptr <Socket> socket, const Address &from) {
        NS_LOG_FUNCTION(this << socket << from);
        auto clientSession = std::make_shared<ClientSessionData>();

        auto nsess = m_socketList.insert(std::make_pair(socket, clientSession));
        nsess.first->second->m_address = from;
        socket->SetRecvCallback(MakeCallback(&Server::ReceivedDataCallback, this));
        StartSendingModel(socket);

        NS_LOG_UNCOND("Accept:" << m_clientSessionManager->ResolveToIdFromServer(socket));

        Ptr <Packet> packet;
        while ((packet = socket->Recv())) {
            if (packet->GetSize() == 0) {
                break; // EOF
            }
        }
    }

    void Server::ServerHandleSend(Ptr <Socket> socket, uint32_t available) {
        m_socket->SetSendCallback(MakeNullCallback < void, Ptr < Socket > , uint32_t > ());

        if (m_sendEvent.IsExpired()) {
            SendModel(socket);
        }
    }

    void Server::StartSendingModel(Ptr <Socket> socket) {
        auto itr = m_socketList.find(socket);
        itr->second->m_bytesModelToReceive = m_bytesModel;
        itr->second->m_bytesModelToSend = m_bytesModel;
        if (m_clientSessionManager->GetRound(socket) == 0)
            itr->second->m_timeBeginSendingModelFromClient;
        else
            itr->second->m_timeBeginSendingModelFromClient = Simulator::Now();
        SendModel(socket);
    }

    void Server::SendModel(Ptr <Socket> socket) {

        //Check if send buffer has available space
        //If not, wait for data ready callback
        auto available = socket->GetTxAvailable();
        if (available == 0) {
            socket->SetSendCallback(MakeCallback(&Server::ServerHandleSend, this));
            return;
        }

        auto itr = m_socketList.find(socket);

        if (itr == m_socketList.end() || itr->second->m_bytesModelToSend == 0) {
            return;
        }

        auto bytes = std::min(std::min(
                itr->second->m_bytesModelToSend, available), m_packetSize);

        auto bytesSent = socket->Send(Create<Packet>(bytes));

        if (bytesSent == -1) {
            return;
        }

        itr->second->m_bytesSent += bytesSent;
        itr->second->m_bytesModelToSend -= bytesSent;

        if (itr->second->m_bytesModelToSend) {
            Time nextTime(Seconds((bytes * 8) /
                                  static_cast<double>(m_dataRate.GetBitRate())));

            m_sendEvent = Simulator::Schedule(nextTime,
                                              &Server::SendModel, this, socket);
        }
        else
          {
            itr->second->m_timeEndSendingModelFromClient=Simulator::Now();
          }
    }

} // Namespace ns3

