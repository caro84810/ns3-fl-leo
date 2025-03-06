#ifndef IOT_EXPERIMENT_H
#define IOT_EXPERIMENT_H

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "fl-sim-interface.h"
#include "fl-client-session.h"

#include <memory>
#include <string>

#include <cstdio>

namespace ns3 {
    /**
  * \ingroup fl-experiment
  * \brief Sets up and runs fl experiments
  */
    class Experiment {
    public:
        Experiment(int numClients, std::string &networkType, int maxPacketSize, double txGain, double modelSize,
                   std::string &dataRate, bool bAsync, FLSimProvider *pflSymProvider, FILE *fp, int round);       
        
        //TODO: Change to run and change packet recieved
        std::map<int, FLSimProvider::Message>
        WeakNetwork(std::map<int, std::shared_ptr<ClientSession> > &packetsReceived, ns3::Time &timeOffset);
        
        /*********ISL修改部份**********/
        
        // 衛星可見性判斷方法
        bool IsSatelliteVisible(const Vector &pos1, const Vector &pos2) const;
        double CalculateDistanceToLine(const Vector &point, const Vector &line_start, const Vector &line_end) const;
        double DotProduct(const Vector &a, const Vector &b) const;
    
    // ISL數據率和延遲計算
        double CalculateISLDataRate(double distance) const;
        

    private:
        /**
        * \brief Set position of node in network
        * \param node        Node to set position of
        * \param radius      Radius location of node
        * \param theta       Angular location of node
        */
        void SetPosition(Ptr <Node> node, double radius, double theta);

        /**
        * \brief Gets position of node
        * \param node   Node to get position of
        * \return       Vector of node position
        */
        Vector GetPosition(Ptr <Node> node);

        /**
        * \brief Sets up wifi network
        */
        NetDeviceContainer Wifi(ns3::NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients);

        /**
        * \brief Sets up ethernet network
        */
        NetDeviceContainer Ethernet(NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients);

        int m_numClients;                 //!< Number of clients in experiment
        std::string m_networkType;        //!< Network type
        int m_maxPacketSize;              //!< Max packet size
        double m_txGain;                  //!< TX gain (for wifi network)
        double m_modelSize;               //!< Size of model
        std::string m_dataRate;           //!< Datarate for server
        bool m_bAsync;                    //!< Indicator bool for whether experiement is async
        FLSimProvider *m_flSymProvider;   //!< pointer to an fl-sim-interface (used to communicate with flsim)
        FILE *m_fp;                       //!< pointer to logfile
        int m_round;                      //!< experiment round
        const char **strings;
        // LEO網路相關函數
        NetDeviceContainer Leo(NodeContainer &c, std::map<int, std::shared_ptr<ClientSession> > &clients);
    
        // LEO網路計算函數
        double CalculateDistance(const Vector &a, const Vector &b) const;
        double CalculateDelay(double distance) const;
        double CalculateDataRate(double distance) const;

        // LEO網路配置參數
        static constexpr double LEO_ALTITUDE = 550.0;  // Starlink衛星高度(km)
        static constexpr double LIGHT_SPEED = 3e5;     // 光速(km/s)
        static constexpr double BASE_DATARATE = 2.2;   // 基礎數據率(Gbps)
        
         // ISL相關常量
         static constexpr double EARTH_RADIUS = 6371.0;  // 地球半徑(km)
         static constexpr double MAX_ISL_RANGE = 5000.0; // 最大ISL通信距離(km)
         static constexpr double ISL_BASE_DATARATE = 2.2; // 基本ISL數據率(Gbps)

    };
}

#endif //IOT_EXPERIMENT_H
