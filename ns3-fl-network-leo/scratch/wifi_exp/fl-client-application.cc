#include "fl-client-application.h"

#include "ns3/internet-module.h"
#include "fl-energy.h"

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE ("ClientApplication");
    NS_OBJECT_ENSURE_REGISTERED (ClientApplication);
    ClientApplication::ClientApplication()
            : m_socket(0),
              m_peer(),
              m_packetSize(0),

              m_bytesModel(0),
              m_dataRate(0),


              m_bytesModelReceived(0),
              m_bytesModelToReceive(0),
              m_timeBeginReceivingModelFromServer(),
              m_timeEndReceivingModelFromServer(),



              m_bytesModelToSend(0),
              m_bytesSent(0),

              m_sendEvent(),
              m_model() {

    }

    TypeId
    ClientApplication::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::ClientApplication")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<ClientApplication>()
                .AddAttribute("DataRate", "The data rate in on state.",
                              DataRateValue(DataRate("500kb/s")),
                              MakeDataRateAccessor(&ClientApplication::m_dataRate),
                              MakeDataRateChecker())

                .AddAttribute("MaxPacketSize",
                              "MaxPacketSize to send to client",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&ClientApplication::m_packetSize),
                              MakeUintegerChecker<uint32_t>())

                .AddAttribute("BytesModel",
                              "Number of bytes in model",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&ClientApplication::m_bytesModel),
                              MakeUintegerChecker<uint32_t>())

                .AddAttribute("BytesSent",
                              "Number of bytes sent from client",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&ClientApplication::m_bytesSent),
                              MakeUintegerChecker<uint32_t>())

                .AddAttribute("BytesReceived",
                              "Number of bytes sent from server",
                              TypeId::ATTR_SGC,
                              UintegerValue(),
                              MakeUintegerAccessor(&ClientApplication::m_bytesModelReceived),
                              MakeUintegerChecker<uint32_t>())

                .AddAttribute("BeginDownlink",
                              "Time begin receiving model from server",
                              TypeId::ATTR_SGC,
                              TimeValue(),
                              MakeTimeAccessor(&ClientApplication::m_timeBeginReceivingModelFromServer),
                              MakeTimeChecker())

                .AddAttribute("EndDownlink",
                              "Time finish receiving model from server",
                              TypeId::ATTR_SGC,
                              TimeValue(),
                              MakeTimeAccessor(&ClientApplication::m_timeEndReceivingModelFromServer),
                              MakeTimeChecker());
        return tid;
    }

    void ClientApplication::NormalClose(Ptr <Socket> socket) {
        NS_LOG_UNCOND(" Close ...");
    }

    void ClientApplication::ErrorClose(Ptr <Socket> socket) {
        NS_LOG_UNCOND("Error Close ...");
    }

    void ClientApplication::HandleReadyToSend(Ptr <Socket> sock, uint32_t available) {
        m_socket->SetSendCallback(MakeNullCallback < void, Ptr < Socket > , uint32_t > ());
        Send(sock);
    }

    void ClientApplication::Send(Ptr <Socket> socket) {
        //If nothing to  send, return
        if (m_bytesModelToSend == 0) {
            return;
        }

        auto available = m_socket->GetTxAvailable();

        if (available) {

            auto bytes = std::min(std::min(m_bytesModelToSend, available), m_packetSize);

            auto bytesSent = socket->Send(Create<Packet>(bytes));

            if (bytesSent == -1) {
                return;
            }


            m_bytesSent += bytesSent;
            m_bytesModelToSend -= bytesSent;


            if (m_bytesModelToSend) {

                Time nextTime(Seconds((bytesSent * 8) /
                                      static_cast<double>(m_dataRate.GetBitRate()))); // Time till next packet

                m_sendEvent = Simulator::Schedule(nextTime,
                                                  &ClientApplication::Send, this, socket);
            } else {
                m_bytesModelToReceive = m_bytesModel;
            }
        } else {
            socket->SetSendCallback(MakeCallback(&ClientApplication::HandleReadyToSend, this));
        }

    }

    void ClientApplication::ConnectionSucceeded(Ptr <Socket> socket) {

        NS_LOG_UNCOND("Client Connected");
        socket->SetRecvCallback(MakeCallback(&ClientApplication::HandleRead, this));

        m_bytesModelToReceive = m_bytesModel;
        m_bytesModelToSend = 0;

        NS_LOG_UNCOND("Client " << (socket->GetNode()->GetId() + 1) << " " << m_bytesModelToReceive);
    }

    void ClientApplication::HandleRead(Ptr <Socket> socket) {

        Ptr <Packet> packet;

        while ((packet = socket->Recv())) {

            if (packet->GetSize() == 0) { //EOF
                break;
            }

            if (m_bytesModelReceived % m_bytesModel == 0 && m_bytesModelReceived != 0) {
                m_timeBeginReceivingModelFromServer = Simulator::Now();
            }

            m_bytesModelReceived += packet->GetSize(); //Increment total bytes received
            m_bytesModelToReceive -= packet->GetSize(); //Decrement bytes expected this round

            if (m_bytesModelToReceive == 0)  //All bytes received, start transmitting
            {
                m_timeEndReceivingModelFromServer = Simulator::Now();

                NS_LOG_UNCOND("Client " << (socket->GetNode()->GetId() + 1) << " " << "recv full model");



                //Todo[] Add a meaningful delay
                auto energy = FLEnergy();
                energy.SetDeviceType("400");
                energy.SetLearningModel("CIFAR-10");
                energy.SetEpochs(5.0);
                Simulator::Schedule(Seconds(energy.CalcComputationTime()), &ClientApplication::StartWriting, this);

            }

        }
    }

    void ClientApplication::StartWriting() {

        m_bytesModelToSend = m_bytesModel;

        Send(m_socket);
    }

    void ClientApplication::ConnectionFailed(Ptr <Socket> socket) {
        NS_LOG_UNCOND("Not Connected ..." << socket->GetNode()->GetId());
    }

    ClientApplication::~ClientApplication() {
    }

    void
    ClientApplication::Setup(Ptr <Socket> socket, Address address, uint32_t packetSize, uint32_t nBytesModel,
                             DataRate dataRate) {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_bytesModel = nBytesModel;
        m_dataRate = dataRate;
    }

    void
    ClientApplication::StartApplication(void) {
    NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        m_model.SetDeviceType("RaspberryPi");
        m_model.SetDataSize(DoubleValue(m_bytesModel));
        m_model.SetPacketSize(DoubleValue(m_packetSize));

        m_model.SetApplication("kNN", DoubleValue(m_packetSize));


       // 檢查 socket 類型
    bool isUdp = (m_socket->GetSocketType() == Socket::NS3_SOCK_DGRAM);
    
    if (!isUdp) {
        // TCP socket 設置
        m_socket->SetCloseCallbacks(
                MakeCallback(&ClientApplication::NormalClose, this),
                MakeCallback(&ClientApplication::ErrorClose, this));
        m_socket->SetConnectCallback(
                MakeCallback(&ClientApplication::ConnectionSucceeded, this),
                MakeCallback(&ClientApplication::ConnectionFailed, this));
        
        m_socket->Bind();
        m_socket->Connect(m_peer);

        /******更加詳細的去處理UDP的SOCKET*******/
    } else {
        // UDP socket 設置
        NS_LOG_UNCOND("UDP Client APP 設置");
        m_socket->Bind();
        
         // 使用UDP專用回調
         m_socket->SetRecvCallback(MakeCallback(&ClientApplication::HandleReadUDP, this));
        
         // 重置計數器
         m_bytesModelReceived = 0;
         m_bytesModelToReceive = m_bytesModel;
         m_timeBeginReceivingModelFromServer = Simulator::Now();
         
         NS_LOG_UNCOND("UDP客戶端準備發送初始包");
          // 發送非常小的初始包
        uint8_t initBuffer[4] = {0x49, 0x4E, 0x49, 0x54}; // "INIT"
        Ptr<Packet> initPacket = Create<Packet>(initBuffer, 4);
        
        int sent = m_socket->SendTo(initPacket, 0, m_peer);
        NS_LOG_UNCOND("UDP客戶端發送初始包結果: " << sent << " bytes");
        
          }
      Simulator::Schedule(MilliSeconds(100), &ClientApplication::RetryConnection, this);
      }

    void
    ClientApplication::StopApplication(void) {


        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }

        if (m_socket) {
            //m_socket->Close ();
        }
    }
    
    /******增加udp處理模式******/

    void ClientApplication::HandleReadUDP(Ptr<Socket> socket) {
    //NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        Ptr<Packet> packet;
        Address from;
        
        while ((packet = socket->RecvFrom(from))) {
            if (packet->GetSize() == 0) break;
            
            // 詳細日誌記錄
            //NS_LOG_UNCOND("UDP接收了 " << packet->GetSize() << " 字節, 總計: " 
                        //<< (m_bytesModelReceived + packet->GetSize()) << "/" << m_bytesModel);
            
            if (m_bytesModelReceived == 0) {
                m_timeBeginReceivingModelFromServer = Simulator::Now();
            }
            
            m_bytesModelReceived += packet->GetSize();
            m_bytesModelToReceive -= packet->GetSize();
            
            // 檢查是否接收完整模型
            if (m_bytesModelToReceive <= 0) {
            
                // 發送確認包
                uint8_t ackBuffer[8] = {0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC};
                Ptr<Packet> ackPacket = Create<Packet>(ackBuffer, 8);
                m_socket->SendTo(ackPacket, 0, m_peer);
                NS_LOG_UNCOND("客戶端發送ACK確認完整接收模型,準備訓練");
                
                m_timeEndReceivingModelFromServer = Simulator::Now();
                
                // 計算訓練延遲
                auto energy = FLEnergy();
                energy.SetDeviceType("400");
                energy.SetLearningModel("CIFAR-10");
                energy.SetEpochs(5.0);
                double compTime = energy.CalcComputationTime();
                
                NS_LOG_UNCOND("預計訓練時間: " << compTime << " 秒");
                Simulator::Schedule(Seconds(compTime), &ClientApplication::SendTrainedModelUDP, this);
            }
        }
    }
    
    // 添加訓練後發送模型的函數
    void ClientApplication::SendTrainedModelUDP() {
        NS_LOG_UNCOND("===== 客戶端開始發送訓練後模型 =====");
        NS_LOG_UNCOND("目標地址: " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4());
        NS_LOG_UNCOND("模型大小: " << m_bytesModel << " 字節");
        
        // 設置要發送的模型大小
        m_bytesModelToSend = m_bytesModel;
        m_bytesSent = 0;
        
        // 開始分包發送
        SendModelChunkUDP();
    }
    
    void ClientApplication::SendModelChunkUDP() {
        if (m_bytesModelToSend <= 0) {
            NS_LOG_UNCOND("模型發送完成");
            return;
        }
        
        uint32_t chunkSize = std::min(m_bytesModelToSend, m_packetSize);
        Ptr<Packet> packet = Create<Packet>(chunkSize);
        
        // 發送數據包
        int sent = m_socket->SendTo(packet, 0, m_peer);
        
        if (sent > 0) {
            m_bytesSent += sent;
            m_bytesModelToSend -= sent;
            
            NS_LOG_UNCOND("已發送 " << m_bytesSent << "/" << m_bytesModel 
                       << " 字節, 剩餘 " << m_bytesModelToSend);
            
            // 安排發送下一個數據包
            Time nextTime(Seconds((chunkSize * 8) / static_cast<double>(m_dataRate.GetBitRate())));
            Simulator::Schedule(nextTime, &ClientApplication::SendModelChunkUDP, this);
        }else{
            NS_LOG_UNCOND("發送失敗，5ms後重試");
            Simulator::Schedule(MilliSeconds(5), &ClientApplication::SendModelChunkUDP, this);
        }
    }
    
    void ClientApplication::RetryConnection() {
    if (m_bytesModelReceived == 0) {
        NS_LOG_UNCOND("重試連接服務器...");
        // 重新發送初始包
        uint8_t initBuffer[10] = {0x49, 0x4E, 0x49, 0x54};
        Ptr<Packet> initPacket = Create<Packet>(initBuffer, 10);
        m_socket->SendTo(initPacket, 0, m_peer);
        
        // 繼續嘗試，直到收到數據
        Simulator::Schedule(MilliSeconds(100), &ClientApplication::RetryConnection, this);
    }
}

}
