/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Emily Ekaireb
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Emily Ekaireb <eekaireb@ucsd.edu>
 */

#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <cstdint>
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include <cstring>
#include<memory>
#include <map>

namespace ns3 {
    class Socket;

    /**
   * \ingroup fl-client-session
   * \brief Client session to store information about the experiment
   */
    class ClientSession {
    public:
        /**
        * \brief Construct client session
        * \param clientID_    Id for client
        * \param radius_      Radius Location for Client
        * \param theta_       Angular Location for Client (Radians)
        */
        ClientSession(int clientID_, double radius_, double theta_) ;

        /**
        * \brief Gets the socket of client
        * \return Socket of client
        */
        Ptr<ns3::Socket> GetClient();

        /**
        * \brief Sets socket of client
        * \param client   Socket to associate with client
        */
        void SetClient(Ptr<ns3::Socket> client);

        /**
        * \brief Get if client is in round
        * \return Bool indicating if client is in round (true == inRound)
        */
        bool GetInRound();

        /**
        * \brief Set if client is in round
        * \param inRound    Bool indicating if client is in round
        */
        void SetInRound(bool inRound);

        /**
        * \brief Get cycle number representing how many times client has participated in round (async)
        * \return cycle number that a client is on
        */
        int GetCycle();

        /**
        * \brief Set cycle number representing how many times client has participated in round (async)
        * \param cycle  cycle number that a client is on
        */
        void SetCycle(int cycle);

        /**
        * \brief Increment by 1 cycle number representing how many times client has participated in round (async)
        */
        void IncrementCycle();

        /**
        * \brief Get radial location of client
        * \return Radial location of client
        */
        double GetRadius();

        /**
        * \brief Get angular location of client
        * \return Angular location of client
        */
        double GetTheta();

        /**
        * \brief Get id of client
        * \return Client id
        */
        int GetClientId();


    private:
        ns3::Ptr<ns3::Socket> m_client;     //!< Socket of client
        double m_radius;                    //!< Radius location of client
        double m_theta;                     //!< Angular location of client
        int m_clientID;                     //!< Client id
        int m_cycle;                        //!< Client cycle for async round
        bool m_inRound;                     //!< Indicates whether client should participate in round
        bool m_dropOut;                     //!< Indicates if client has dropped out of round
    };


}
#endif
