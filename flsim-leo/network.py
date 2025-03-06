#from py_interface import *
from ctypes import *
import socket
import struct
import subprocess
import logging

TCP_IP = '127.0.0.1'
TCP_PORT = 8080
PATH='../ns3-fl-network'
PROGRAM='wifi_exp'

class Network(object):
    def __init__(self, config):
        self.config = config
        self.num_clients = self.config.clients.total
        self.network_type = self.config.network.type

        proc = subprocess.Popen('./waf build', shell=True, stdout=subprocess.PIPE,
                                universal_newlines=True, cwd=PATH)
        proc.wait()
        if proc.returncode != 0:
            exit(-1)

        command = './waf --run "' + PROGRAM + ' --NumClients=' + str(self.num_clients) + ' --NetworkType=' + self.network_type
        command += ' --ModelSize=' + str(self.config.model.size)
        '''print(self.config.network)
        for net in self.config.network:
            if net == self.network_type:
                print(net.items())'''

        if self.network_type == 'wifi':
            command += ' --TxGain=' + str(self.config.network.wifi['tx_gain'])
            command += ' --MaxPacketSize=' + str(self.config.network.wifi['max_packet_size'])
        else: # else assume ethernet
            command += ' --MaxPacketSize=' + str(self.config.network.ethernet['max_packet_size'])

        command += " --LearningModel=" + str(self.config.server)

        command += '"'
        print(command)

        proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE,
                                universal_newlines=True, cwd=PATH)


    def parse_clients(self, clients):
        clients_to_send = [0 for _ in range(self.num_clients)]
        for client in clients:
            clients_to_send[client.client_id] = 1
        return clients_to_send

    def connect(self):
        self.s = socket.create_connection((TCP_IP, TCP_PORT,), timeout=60)

    def sendRequest(self, *, requestType: int, array: list):
        print("sending")
        print(array)
        message = struct.pack("II", requestType, len(array))
        self.s.send(message)
        # for the total number of clients
        # is the index in lit at client.id equal
        for ele in array:
            self.s.send(struct.pack("I", ele))

        resp = self.s.recv(8)
        print("resp")
        print(resp)
        if len(resp) < 8:
            print(len(resp), resp)
        command, nItems = struct.unpack("II", resp)
        ret = {}
        for i in range(nItems):
            dr = self.s.recv(8 * 3)
            eid, roundTime, throughput = struct.unpack("Qdd", dr)
            temp = {"roundTime": roundTime, "throughput": throughput}
            ret[eid] = temp
        return ret

    def sendAsyncRequest(self, *, requestType: int, array: list):
        print("sending")
        print(array)
        message = struct.pack("II",requestType , len(array))
        self.s.send(message)
        # for the total number of clients
        # is the index in lit at client.id equal
        for ele in array:
            self.s.send(struct.pack("I", ele))

    def readAsyncResponse(self):
        try:
            logging.debug("等待NS3響應...")
            resp = self.s.recv(8)
            logging.info("收到響應數據，長度=%d bytes", len(resp))
            
            if len(resp) < 8:
                logging.warning("響應數據不足8字節: %r", resp)
                if len(resp) == 0:
                    logging.info("收到空響應，返回'end'信號")
                    return 'end'               
                    
            command, nItems = struct.unpack("II", resp)
            logging.info("解析命令: %d, 項目數: %d", command, nItems)

            if command == 3:
                logging.info("收到ENDSIM命令")
                return 'end'
            
            ret = {}
            for i in range(nItems):
                try:
                    dr = self.s.recv(8 * 4)
                    logging.debug("項目數據長度: %d bytes", len(dr))
                    
                    if len(dr) >= 8 * 4:
                        eid, startTime, endTime, throughput = struct.unpack("Qddd", dr)
                        logging.info("客戶端 %d: start=%f, end=%f, throughput=%f", 
                               eid, startTime, endTime, throughput)
                        ret[eid] ={
                            "startTime": startTime, 
                            "endTime": endTime, 
                            "throughput": throughput
                        }
                    else:
                        logging.error("接收數據不完整，長度只有%d bytes", len(dr))
                
                except Exception as item_err:
                    logging.exception("處理項目%d數據時出錯: %s", i, str(item_err))    
                                                                       
            return ret
        except Exception as e:
            logging.exception("讀取異步回應時出錯: %s", str(e))
            return 'end'  # 出錯時返回'end'以避免卡住


    def disconnect(self):
        # self.sendAsyncRequest(requestType=2, array=[])
        self.s.close()

