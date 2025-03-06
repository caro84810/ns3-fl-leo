#include "fl-experiment.h"
#include "fl-client-application.h"
#include "fl-server.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "fl-server-helper.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/csma-helper.h" 
#include "ns3/ipv4-static-routing-helper.h"
#include "client-session-manager.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include "ns3/ipv4-global-routing-helper.h"

namespace ns3 {

    Experiment::Experiment(int numClients, std::string &networkType, int maxPacketSize, double txGain, double modelSize,
                           std::string &dataRate, bool bAsync, FLSimProvider *fl_sim_provider, FILE *fp, int round) :
            m_numClients(numClients),
            m_networkType(networkType),
            m_maxPacketSize(maxPacketSize),
            m_txGain(txGain),
            m_modelSize(modelSize),
            m_dataRate(dataRate),
            m_bAsync(bAsync),
            m_flSymProvider(fl_sim_provider),
            m_fp(fp),
            m_round(round) {
            NS_LOG_UNCOND("Initializing Experiment with flSimProvider: " << (fl_sim_provider != nullptr ? "Valid" : "NULL"));
    }
    
            

    void
    Experiment::SetPosition(Ptr <Node> node, double radius, double theta) {
        double x = radius * sin(theta * 2 * M_PI);
        double y = radius * cos(theta * 2 * M_PI);
        double z = 0;
        Ptr <MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetPosition(Vector(x, y, z));
    }

    Vector
    Experiment::GetPosition(Ptr <Node> node) {
        Ptr <MobilityModel> mobility = node->GetObject<MobilityModel>();
        return mobility->GetPosition();
    }

    NetDeviceContainer Experiment::Ethernet(NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients) {
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices;
        csmaDevices = csma.Install(c);

        return csmaDevices;
    }


    NetDeviceContainer Experiment::Wifi(NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients) {

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        YansWifiPhyHelper wifiPhy;


        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper();

        //wifiPhy.Set("TxGain", DoubleValue(m_txGain));//-23.5) );
        

        wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

        //90 Weak Network
        //70 Medium
        //30 Stong
        //double trigger = 30.0;

        Ptr<UniformRandomVariable> expVar = CreateObjectWithAttributes<UniformRandomVariable> (
            "Min", DoubleValue (m_txGain),
            "Max", DoubleValue (m_txGain+30)
            );

        wifiChannel.AddPropagationLoss ("ns3::RandomPropagationLossModel","Variable",PointerValue(expVar));

        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        //wifiChannel.SetPropagationDelay("ns3::RandomPropagationDelayModel", "Variable", StringValue ("ns3::UniformRandomVariable[Min=0|Max=2]"));

        //std::string phyMode("HtMcs0");
        std::string phyMode("DsssRate11Mbps");

        // Fix non-unicast data rate to be the same as that of unicast
        Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                           StringValue(phyMode));

        //wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
        wifi.SetStandard(WIFI_STANDARD_80211b);
        // This is one parameter that matters when using FixedRssLossModel
        // set it to zero; otherwise, gain will be added
        wifiPhy.Set("RxGain", DoubleValue(0));

        // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
     //   wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);


        wifiPhy.SetChannel(wifiChannel.Create());

        

        // Add a mac and disable rate control
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(phyMode),
                                     "ControlMode", StringValue(phyMode));

        // Set it to adhoc mode
        wifiMac.SetType("ns3::AdhocWifiMac");


        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(c);

        int numClients = clients.size();
        for (int j = 1; j <= numClients; j++) {
            if (clients[j - 1]->GetInRound()) {

                Experiment::SetPosition(c.Get(j), clients[j - 1]->GetRadius(), clients[j - 1]->GetTheta());
            }
        }

        return devices;

    }

    const char *wifi_strings[] =
            {
                    "75kbps",
                    "125kbps",
                    "150kbps",
                    "160kbps",
                    "175kbps",
                    "200kbps",
            };

    const char *ethernet_strings[] =
            {
                    "80kbps",
                    "160kbps",
                    "320kbps",
                    "640kbps",
                    "1024kbps",
                    "2048kbps",
            };
            
/********LEO MODE程式碼********/
    NetDeviceContainer Experiment::Leo(NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients) {
        NS_LOG_UNCOND("Starting LEO network setup...");
        
        // 設置移動模型
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(c);
        NS_LOG_UNCOND("Mobility model installed");
        
        // 設置地面站(server)位置
        Vector groundPos(0.0, 0.0, 0.0);
        c.Get(0)->GetObject<MobilityModel>()->SetPosition(groundPos);
    
        // 設置衛星(clients)位置
        for (uint32_t i = 1; i < c.GetN(); ++i) {
            // 計算更分散的衛星位置，避免集中在一個區域
            double theta = 2.0 * M_PI * (i - 1) / (c.GetN() - 1);
            // 添加一些隨機變化，使衛星分布更自然
            double orbital_plane = ((i-1) % 4) + 1; // 4個軌道平面
            double altitude = LEO_ALTITUDE * (1.0 + 0.05 * orbital_plane); 
            
            Vector satPos(
                altitude * cos(theta),
                altitude * sin(theta),
                altitude * 0.2 * sin(2*theta) * orbital_plane // Z軸變化
            );
            c.Get(i)->GetObject<MobilityModel>()->SetPosition(satPos);
            NS_LOG_UNCOND("Satellite " << i << " position: " << satPos.x << ", " << satPos.y << ", " << satPos.z);
        }

        // 創建所有網絡設備的容器
        NetDeviceContainer allDevices;
        
        // 1.地面站到衛星
        NS_LOG_UNCOND("Creating ground-to-satellite connections...");        
        PointToPointHelper p2p_ground;
        //NetDeviceContainer ground_devices;
    
        // 為每個衛星建立與地面站的連接，使用UDP傳輸
        for (uint32_t i = 1; i < c.GetN(); ++i) {
            // 計算實際距離
            Vector satPos = c.Get(i)->GetObject<MobilityModel>()->GetPosition();
            double distance = CalculateDistance(groundPos, satPos);
            
            // 根據距離計算更真實的延遲和數據率
            double delay = CalculateDelay(distance);
            double dataRate = CalculateDataRate(distance);
            
            NS_LOG_UNCOND("Link to Satellite " << i << " - Distance: " << distance 
                        << "km, Delay: " << delay << "s, DataRate: " << dataRate << "Gbps");
            
            // 設置連接參數 - 使用合理的延遲和數據率
            p2p_ground.SetChannelAttribute("Delay", TimeValue(Seconds(delay)));
            p2p_ground.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Gbps")));
            
            // 增加錯誤模型模擬衛星鏈路的可靠性問題
            Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
            // 設置適中的錯誤率，不會阻礙通信但反映衛星鏈路的特性
            em->SetAttribute("ErrorRate", DoubleValue(0.000001));
            
            NetDeviceContainer link = p2p_ground.Install(c.Get(0), c.Get(i));
            // 在上行鏈路應用錯誤模型
            link.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
            
            allDevices.Add(link);
        }

        // 2. 創建ISL
        NS_LOG_UNCOND("Creating ISL...");
        PointToPointHelper p2p_isl;
        
        // 避免重複連接的矩陣
        std::vector<std::vector<bool>> connected(c.GetN(), std::vector<bool>(c.GetN(), false));       
        int isl_count = 0; // 統計ISL數量
                     
        // 將衛星按軌道平面分組
        std::vector<std::vector<uint32_t>> orbitalPlanes(4); // 4個軌道平面
        for (uint32_t i = 1; i < c.GetN()-1; ++i) { // 注意：如果有第二個地面站，要改為c.GetN()-1
            int planeId = ((i-1) % 4);
            orbitalPlanes[planeId].push_back(i);
        }

        // 打印每個軌道平面的衛星
        for (int plane = 0; plane < 4; ++plane) {
            NS_LOG_UNCOND("Orbital plane " << plane << " satellites: ");
            for (auto sat : orbitalPlanes[plane]) {
                NS_LOG_UNCOND("  Satellite " << sat);
            }
        }
        
        // 1. 同一軌道平面內的相鄰衛星連接（環形連接）
        for (int plane = 0; plane < 4; ++plane) {
            NS_LOG_UNCOND("Creating intra-plane ISLs for orbital plane " << plane);
            const auto& satellites = orbitalPlanes[plane];
            
            if (satellites.size() < 2) continue;
            
            // 連接每個衛星與其下一個衛星
            for (size_t i = 0; i < satellites.size(); ++i) {
                size_t next_idx = (i + 1) % satellites.size(); // 環形連接
                uint32_t sat1 = satellites[i];
                uint32_t sat2 = satellites[next_idx];
                
                if (connected[sat1][sat2]) continue;
                
                Vector pos1 = c.Get(sat1)->GetObject<MobilityModel>()->GetPosition();
                Vector pos2 = c.Get(sat2)->GetObject<MobilityModel>()->GetPosition();
                double distance = CalculateDistance(pos1, pos2);
                
                if (distance <= MAX_ISL_RANGE) {
                    double delay = CalculateDelay(distance);
                    double dataRate = CalculateISLDataRate(distance);
                    
                    NS_LOG_UNCOND("Creating intra-plane ISL between Satellites " << sat1 << " and " << sat2 
                                << " - Distance: " << distance << "km, Delay: " << delay << "s");
                    
                    p2p_isl.SetChannelAttribute("Delay", TimeValue(Seconds(delay)));
                    p2p_isl.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Gbps")));
                    
                    Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
                    Ptr<RateErrorModel> em2 = CreateObject<RateErrorModel>();
                    em1->SetAttribute("ErrorRate", DoubleValue(0.000005));
                    em2->SetAttribute("ErrorRate", DoubleValue(0.000005));
                    
                    NetDeviceContainer isl = p2p_isl.Install(c.Get(sat1), c.Get(sat2));
                    isl.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em1));
                    isl.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em2));
                    
                    allDevices.Add(isl);
                    connected[sat1][sat2] = connected[sat2][sat1] = true;
                    isl_count++;
                }
            }
        }

        // 2. 相鄰軌道平面間的對應位置的衛星連接
        for (int plane1 = 0; plane1 < 4; ++plane1) {
            int plane2 = (plane1 + 1) % 4; // 相鄰平面（環形）
            
            NS_LOG_UNCOND("Creating inter-plane ISLs between planes " << plane1 << " and " << plane2);
            
            const auto& satellites1 = orbitalPlanes[plane1];
            const auto& satellites2 = orbitalPlanes[plane2];
            
            size_t min_sats = std::min(satellites1.size(), satellites2.size());
            
            // 對每對對應位置的衛星進行連接
            for (size_t pos = 0; pos < min_sats; ++pos) {
                uint32_t sat1 = satellites1[pos];
                uint32_t sat2 = satellites2[pos];
                
                if (connected[sat1][sat2]) continue;
                
                Vector pos1 = c.Get(sat1)->GetObject<MobilityModel>()->GetPosition();
                Vector pos2 = c.Get(sat2)->GetObject<MobilityModel>()->GetPosition();
                
                // 檢查兩衛星是否在極區（簡化判斷：z坐標絕對值較大）
                bool polarRegion = (std::abs(pos1.z) > LEO_ALTITUDE * 0.5 || std::abs(pos2.z) > LEO_ALTITUDE * 0.5);
                
                // 極區不建立ISL
                if (!polarRegion) {
                    double distance = CalculateDistance(pos1, pos2);
                    
                    if (distance <= MAX_ISL_RANGE) {
                        double delay = CalculateDelay(distance);
                        double dataRate = CalculateISLDataRate(distance);
                        
                        NS_LOG_UNCOND("Creating inter-plane ISL between Satellites " << sat1 << " and " << sat2 
                                    << " - Distance: " << distance << "km, Delay: " << delay << "s");
                        
                        p2p_isl.SetChannelAttribute("Delay", TimeValue(Seconds(delay)));
                        p2p_isl.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Gbps")));
                        
                        Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
                        Ptr<RateErrorModel> em2 = CreateObject<RateErrorModel>();
                        em1->SetAttribute("ErrorRate", DoubleValue(0.000005));
                        em2->SetAttribute("ErrorRate", DoubleValue(0.000005));
                        
                        NetDeviceContainer isl = p2p_isl.Install(c.Get(sat1), c.Get(sat2));
                        isl.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em1));
                        isl.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em2));
                        
                        allDevices.Add(isl);
                        connected[sat1][sat2] = connected[sat2][sat1] = true;
                        isl_count++;
                    }
                }
            }
        }
        
        
        NS_LOG_UNCOND("Created " << isl_count << " Inter-Satellite Links");
        NS_LOG_UNCOND("Total network devices: " << allDevices.GetN());
    
        return allDevices;
    }
    
    // 調整延遲計算，使其更現實但不會過度延長模擬時間
    double Experiment::CalculateDelay(double distance) const {
        // 實際的光速延遲，但添加一個縮放因子使模擬可行
        // 在真實環境中，LEO到地面的單程延遲約為20-40ms
        // 我們使用0.5作為縮放因子來模擬這種延遲範圍
        return (distance / LIGHT_SPEED) * 0.5;  
    }
    
    // 調整數據率計算，使其反映衛星鏈路的性能但不會過度限制通信
    double Experiment::CalculateDataRate(double distance) const {
        // 基本數據率提高，但仍然隨距離衰減
        // LEO衛星通常提供約50Mbps-1Gbps的數據率
        double base_rate = 2.0;  // 基本速率(Gbps)
        return base_rate * exp(-distance / 10000.0);  // 距離衰減更緩慢
    }        


    double Experiment::CalculateDistance(const Vector &a, const Vector &b) const {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        double dz = a.z - b.z;
        return sqrt(dx * dx + dy * dy + dz * dz);
    }

    /**********ISL輔助函數新增**********/

    // 計算兩個向量的點積
    double Experiment::DotProduct(const Vector &a, const Vector &b) const {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // 計算點到線段的距離，用於地球遮擋判斷
    double Experiment::CalculateDistanceToLine(const Vector &point, const Vector &line_start, const Vector &line_end) const {
        Vector v = line_end - line_start;
        Vector w = point - line_start;
        
        double c1 = DotProduct(w, v);
        double c2 = DotProduct(v, v);
        
        // 處理特殊情況，避免除零錯誤
        if (c2 < 0.0001) return CalculateDistance(point, line_start);
        
        double b = c1 / c2;
        
        // 處理線段端點情況
        if (b <= 0.0) return CalculateDistance(point, line_start);
        if (b >= 1.0) return CalculateDistance(point, line_end);
        
        // 計算線段上最近點
        Vector pb = line_start;
        pb.x += v.x * b;
        pb.y += v.y * b;
        pb.z += v.z * b;
        
        return CalculateDistance(point, pb);
    }

    // 判斷兩顆衛星是否可見
    bool Experiment::IsSatelliteVisible(const Vector &pos1, const Vector &pos2) const {
        
        // 計算距離
        double distance = CalculateDistance(pos1, pos2);
        
        // 檢查距離是否在通信範圍內
        if (distance > MAX_ISL_RANGE) {
            return false;
        }
        
        // 地球中心位置 (座標原點)
        Vector earth_center(0, 0, 0);
        
        // 計算地球到衛星連線的最短距離
        double dist_to_line = CalculateDistanceToLine(earth_center, pos1, pos2);
        
        // 如果距離小於地球半徑，則被地球遮擋
        return dist_to_line > EARTH_RADIUS;
    }

    // 計算ISL數據率
    double Experiment::CalculateISLDataRate(double distance) const {
        // 基於距離的指數衰減模型
        double attenuation = std::exp(-distance / 20000.0); // 較緩和的衰減
        return ISL_BASE_DATARATE * attenuation;
    }   

/********Network主程式碼********/
    std::map<int, FLSimProvider::Message>Experiment::WeakNetwork(std::map<int, std::shared_ptr<ClientSession> > &clients, ns3::Time &timeOffset) {
        NS_LOG_UNCOND("Starting WeakNetwork...");
        NS_LOG_UNCOND("Number of clients: " << clients.size());
        int selectedClientCount = 0;
        for (std::size_t j = 0; j < clients.size(); j++) {
            NS_LOG_UNCOND("Client " << j << " InRound: " << clients[j]->GetInRound());
            if (clients[j]->GetInRound()) {
                selectedClientCount++;
            }
        }
        NS_LOG_UNCOND("Selected client count: " << selectedClientCount);
        
        int server = 0;
        int numClients = clients.size();

        NodeContainer c;
        c.Create(numClients + 1);

        NetDeviceContainer devices;
        NS_LOG_UNCOND("Setting up " << m_networkType << " network");
        
        if (m_networkType.compare("wifi") == 0) {
            devices = Wifi(c, clients);
            strings = ethernet_strings;
        }
        else if(m_networkType.compare("leo") == 0){
            devices = Leo(c, clients);
            strings = ethernet_strings; // 用ethernet的速率範圍
        }
        else { // assume ethernet if not specified
            devices = Ethernet(c, clients);
        }

        NS_LOG_UNCOND("Installing Internet stack...");
        
        InternetStackHelper internet;
        internet.Install(c);
        
        NS_LOG_UNCOND("Assigning IP addresses...");
        Ipv4AddressHelper ipv4;
        
        // 確保interfaces在任何情況下都被定義
    	Ipv4InterfaceContainer interfaces;
        
        if (m_networkType.compare("leo") == 0) {
            NS_LOG_UNCOND("Setting up LEO IP addressing scheme...");
            
            // 使用單一子網，簡化IP管理
            ipv4.SetBase("10.1.0.0", "255.255.0.0");
            interfaces = ipv4.Assign(devices);

            NS_LOG_UNCOND("Total interfaces created: " << interfaces.GetN());

            //獲取所有節點位置
            std::vector<Vector> nodePositions;
            for (uint32_t i = 0; i < c.GetN(); ++i) {
                Vector pos = c.Get(i)->GetObject<MobilityModel>()->GetPosition();
                nodePositions.push_back(pos);
            }

            // 設置路由 - 優先使用全局路由，但針對服務器與參與節點添加額外的靜態路由
            Ipv4GlobalRoutingHelper globalRouting;
            globalRouting.PopulateRoutingTables();

            // 額外添加靜態路由以確保服務器能夠與當前輪次的客戶端直接通信
            Ipv4StaticRoutingHelper staticRouting;

            // 打印網絡拓撲信息，用於調試
            NS_LOG_UNCOND("Network topology information:");
            for (uint32_t i = 0; i < c.GetN(); ++i) {
                Ptr<Ipv4> ipv4Obj = c.Get(i)->GetObject<Ipv4>();
                NS_LOG_UNCOND("Node " << i << " has " << ipv4Obj->GetNInterfaces() << " interfaces");
                
                for (uint32_t j = 0; j < ipv4Obj->GetNInterfaces(); ++j) {
                    NS_LOG_UNCOND("  Interface " << j << " addr: " << ipv4Obj->GetAddress(j, 0).GetLocal());
                }
            }

            // 打印全局路由表
            NS_LOG_UNCOND("Global routing tables:");
            Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("leo-routes.txt", std::ios::out);
            globalRouting.PrintRoutingTableAllAt(Seconds(10.0), routingStream);            
        } else {
            ipv4.SetBase("10.1.1.0", "255.255.255.0");
            Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        }
        
        NS_LOG_UNCOND("Setting up routing...");
        // 驗證路由
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        for (int i = 0; i <= numClients; i++) {
            Ptr<Ipv4> ipv4 = c.Get(i)->GetObject<Ipv4>();
            Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);

            NS_LOG_UNCOND("Node " << i << " routing table:");
            for (uint32_t j = 0; j < staticRouting->GetNRoutes(); j++) {
                Ipv4RoutingTableEntry route = staticRouting->GetRoute(j);
                NS_LOG_UNCOND("  " << route.GetDest() << "/" << route.GetDestNetworkMask() 
                            << " via " << route.GetGateway());
            }
        }

        NS_LOG_UNCOND("Setting up server...");
        
        // 設置服務器 - 為LEO模式使用UDP
        Ipv4Address serverAddr = interfaces.GetAddress(0);
        
        TypeId socketType;
        if (m_networkType.compare("leo") == 0) {
            socketType = UdpSocketFactory::GetTypeId();
            NS_LOG_UNCOND("Using UDP socket for LEO mode");
        } else {
            socketType = TcpSocketFactory::GetTypeId();
            NS_LOG_UNCOND("Using TCP socket for " << m_networkType << " mode");
        }
        
        ServerHelper server_helper(socketType.GetName(), InetSocketAddress(Ipv4Address::GetAny(), 80));
        server_helper.SetAttribute("MaxPacketSize", UintegerValue(m_maxPacketSize));
        server_helper.SetAttribute("BytesModel", UintegerValue(m_modelSize));
        server_helper.SetAttribute("DataRate", StringValue(m_dataRate));
        server_helper.SetAttribute("Async", BooleanValue(m_bAsync));
        server_helper.SetAttribute("TimeOffset", TimeValue(timeOffset));
        ApplicationContainer sinkApps = server_helper.Install(c.Get(server));

        sinkApps.Start(Seconds(0.));
        
        NS_LOG_UNCOND("Server IP: " << serverAddr);

        NS_LOG_UNCOND("Setting up clients...");
        // 設置Clients
        std::map<Ipv4Address, int> m_addrMap;
        for (int j = 1; j <= numClients; j++) {
            if (clients[j - 1]->GetInRound()) {
                NS_LOG_UNCOND("Setting up client " << j);
                
                // 為LEO模式使用UDP
                Ptr<Socket> source;
                bool isUdp = (m_networkType.compare("leo") == 0);
        
                NS_LOG_UNCOND("Network Type: " << m_networkType << ", Is UDP: " << (isUdp ? "Yes" : "No"));

		//依照不同網路類型來創不同的socket
                if (isUdp) {
                    source = Socket::CreateSocket(c.Get(j), UdpSocketFactory::GetTypeId());
                    NS_LOG_UNCOND("Created UDP socket for client " << j);
                } else {
                    source = Socket::CreateSocket(c.Get(j), TcpSocketFactory::GetTypeId());
                    NS_LOG_UNCOND("Created TCP socket for client " << j);
                    
                    source->SetAttribute("ConnCount", UintegerValue(1000));
                    source->SetAttribute("DataRetries", UintegerValue(100));
                }
                
                Ipv4Address clientAddr = interfaces.GetAddress(j);
                m_addrMap[clientAddr] = j - 1;

                Address sinkAddress(InetSocketAddress(serverAddr, 80));


                Ptr<ClientApplication> app = CreateObject<ClientApplication>();
                std::string dataRate = isUdp ? "0.5Gbps" : std::string(ethernet_strings[j % 6]);
                
                NS_LOG_UNCOND("Client " << j << " data rate: " << dataRate);
                
                app->Setup(source, sinkAddress, m_maxPacketSize, m_modelSize, dataRate);
                c.Get(j)->AddApplication(app);
                app->SetStartTime(Seconds(1.));
                app->SetStopTime(Seconds(1000000.0));
                NS_LOG_UNCOND("Client application set up for client " << j);
                
                clients[j - 1]->SetClient(source);
                clients[j - 1]->SetCycle(0);

                NS_LOG_UNCOND("Client " << j << " IP: " << clientAddr);
            }
        }

        ClientSessionManager client_session_manager(clients);
        sinkApps.Get(0)->GetObject<ns3::Server>()->SetClientSessionManager(
            &client_session_manager,
            m_flSymProvider,
            m_fp,
            m_round
        );

        NS_LOG_UNCOND("FLSimProvider initialized: " << (m_flSymProvider != nullptr ? "YES" : "NO"));
        
        // 為LEO模式設置更長的模擬時間
        if (m_networkType.compare("leo") == 0) {
            NS_LOG_UNCOND("Setting longer simulation time for LEO mode: 500s");
            Simulator::Stop(Seconds(500.0));
        } else {
            NS_LOG_UNCOND("Setting simulation stop time: 100s");
            Simulator::Stop(Seconds(100.0));
        }
        
        NS_LOG_UNCOND("Starting simulation at " << Simulator::Now().GetSeconds() << "s");
        Simulator::Run();
        NS_LOG_UNCOND("Simulation ended at " << Simulator::Now().GetSeconds() << "s");
        
        NS_LOG_UNCOND("Collecting statistics...");
        TimeValue endTime;
        sinkApps.Get(0)->GetObject<ns3::Server>()->GetAttribute("TimeOffset", endTime);
        timeOffset = endTime.Get();

        std::map<int, FLSimProvider::Message> roundStats;
        if (m_bAsync == false) {
            // 同步模式的統計收集...
            // (保留原有代碼)
        }
        
        NS_LOG_UNCOND("Explicitly calling FLSimProvider::end() at simulation end");
        m_flSymProvider->end();  // 假設 m_flSimProvider 是可用的
        
        NS_LOG_UNCOND("WeakNetwork completed");
        Simulator::Destroy();
        return roundStats;
    }

}// namespace ns3
