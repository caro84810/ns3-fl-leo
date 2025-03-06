#include "fl-sim-interface.h"

namespace ns3 {
    void FLSimProvider::waitForConnection() {
    NS_LOG_UNCOND("===== 函數名: " << __FUNCTION__ << " =====");
        m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_server_fd < 0) {
            NS_LOG_UNCOND("Could not create a socket");
            exit(-1);
        }
        int opt = 1;
        setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt));

        m_address.sin_family = AF_INET;
        m_address.sin_addr.s_addr = INADDR_ANY;
        m_address.sin_port = htons(m_port);

        if (bind(m_server_fd, (struct sockaddr *) &m_address, sizeof(m_address)) == -1) {
            NS_LOG_UNCOND("Could not bind to port");
        }
        listen(m_server_fd, 3);

        int addrlen = sizeof(m_address);
        m_new_socket = accept(m_server_fd, (struct sockaddr *) &m_address,
                            (socklen_t * ) & addrlen);

        if (m_new_socket < 0) {
            exit(-1);
        }
    }

    FLSimProvider::COMMAND::Type FLSimProvider::recv(std::map<int, std::shared_ptr<ClientSession> > &packetsReceived) {
        COMMAND c;
        int len = read(m_new_socket, (char *) &c, sizeof(c));

        if (len != sizeof(COMMAND)) {
            if (len != 0) {
                NS_LOG_UNCOND("Invalid Command: Len(" << len << ")!=(" << sizeof(COMMAND) << ")");
            } else {
                NS_LOG_UNCOND("Socket closed by Python");
            }
            close(m_new_socket);
            return COMMAND::Type::EXIT;
        }

        if (c.command == COMMAND::Type::EXIT) {
            NS_LOG_UNCOND("Exit Called");
            close(m_new_socket);
            return COMMAND::Type::EXIT;
        } else if (c.command != COMMAND::Type::RUN_SIMULATION) {
            NS_LOG_UNCOND("Invalid command");
            close(m_new_socket);
            return COMMAND::Type::EXIT;
        } else if (packetsReceived.size() != c.nItems) {
            NS_LOG_UNCOND("Invalid number of clients");
            close(m_new_socket);
            return COMMAND::Type::EXIT;
        }


        int i = 0;
        for (auto it = packetsReceived.begin(); it != packetsReceived.end(); it++, i++) {
            uint32_t temp;

            if (sizeof(temp) != read(m_new_socket, (char *) &temp, sizeof(temp))) {
                NS_LOG_UNCOND("Invalid valid length received");
                return COMMAND::Type::EXIT;
            }

            it->second->SetInRound((temp == 0) ? false : true);


        }


        return c.command;
    }

    void FLSimProvider::Close() {
        close(m_new_socket);
    }

    void FLSimProvider::send(AsyncMessage *pMessage) {
        COMMAND r;
        r.command = COMMAND::Type::RESPONSE;
        r.nItems = 1;
        // 將消息寫入前先進行日誌記錄
    NS_LOG_UNCOND("Sending async message to Python: id=" << pMessage->id 
                 << ", startTime=" << pMessage->startTime
                 << ", endTime=" << pMessage->endTime 
                 << ", throughput=" << pMessage->throughput);
    
    // 發送命令頭
    ssize_t written = write(m_new_socket, (char *) &r, sizeof(r));
    if (written != sizeof(r)) {
        NS_LOG_UNCOND("Error writing command to socket: wrote " << written << " bytes, expected " << sizeof(r));
        return;
    }
    
    // 發送消息內容
    written = write(m_new_socket, pMessage, sizeof(AsyncMessage));
    if (written != sizeof(AsyncMessage)) {
        NS_LOG_UNCOND("Error writing message to socket: wrote " << written << " bytes, expected " << sizeof(AsyncMessage));
    } else {
        NS_LOG_UNCOND("Successfully sent " << written << " bytes of async message");
    }
    }

    void FLSimProvider::end() {
        COMMAND r;
        r.command = COMMAND::Type::ENDSIM;
        r.nItems = 0;
        
        NS_LOG_UNCOND("Sending ENDSIM command to Python");
    
        // 發送終止命令
        ssize_t written = write(m_new_socket, (char *) &r, sizeof(r));
        if (written != sizeof(r)) {
            NS_LOG_UNCOND("Error writing ENDSIM command: wrote " << written << " bytes, expected " << sizeof(r));
        } else {
            NS_LOG_UNCOND("Successfully sent ENDSIM command");
        }
    }


    void FLSimProvider::send(std::map<int, Message> &roundTime) {
        //NS_LOG_FUNCTION(this);

        COMMAND r;
        r.command = COMMAND::Type::RESPONSE;
        r.nItems = roundTime.size();
        write(m_new_socket, (char *) &r, sizeof(r));

        for (auto it = roundTime.begin(); it != roundTime.end(); it++) {
            Message &temp = it->second;
            temp.id = it->first;
            write(m_new_socket, (char *) &it->second, sizeof(Message));
        }

        roundTime.clear();
    }
}
