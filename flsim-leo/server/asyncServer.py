import logging
import pickle
import random
import math
from threading import Thread
import torch
import time
from queue import PriorityQueue
import os
from server import Server
from network import Network
from .record import Record, Profile


class AsyncServer(Server):
    """Asynchronous federated learning server."""

    def load_model(self):
        import fl_model  # pylint: disable=import-error

        model_path = self.config.paths.model
        model_type = self.config.model

        logging.info('Model: {}'.format(model_type))

        # Set up global model
        self.model = fl_model.Net()
        self.async_save_model(self.model, model_path, 0.0)

        # Extract flattened weights (if applicable)
        if self.config.paths.reports:
            self.saved_reports = {}
            self.save_reports(0, [])  # Save initial model

    def make_clients(self, num_clients):
        super().make_clients(num_clients)

        # Set link speed for clients
        speed = []
        for client in self.clients:
            client.set_link(self.config)
            speed.append(client.speed_mean)

        logging.info('Speed distribution: {} Kbps'.format([s for s in speed]))

        # Initiate client profile of loss and delay
        self.profile = Profile(num_clients)
        if self.config.data.IID == False:
            self.profile.set_primary_label([client.pref for client in self.clients])

    # Run asynchronous federated learning
    def run(self):
        rounds = self.config.fl.rounds
        target_accuracy = self.config.fl.target_accuracy
        reports_path = self.config.paths.reports

        # Init async parameters
        self.alpha = self.config.sync.alpha
        self.staleness_func = self.config.sync.staleness_func

        network = Network(self.config)  # create ns3 network/start ns3 program
        # dummy call to access

        # Init self accuracy records
        self.records = Record()

        if target_accuracy:
            logging.info('Training: {} rounds or {}% accuracy\n'.format(
                rounds, 100 * target_accuracy))
        else:
            logging.info('Training: {} rounds\n'.format(rounds))

        # Perform rounds of federated learning
        T_old = 0.0

        #connect to network
        time.sleep(1)
        network.connect()

        f = open("dropout.txt", "a")

        for round in range(1, rounds + 1):
            logging.info('**** Round {}/{} ****'.format(round, rounds))
            f.write('**** Round {}/{} ****'.format(round, rounds))
            f.write('\n')
            f.flush()
            # Perform async rounds of federated learning with certain
            # grouping strategy
            self.rm_old_models(self.config.paths.model, T_old)
            accuracy, T_new = self.async_round(round, T_old, network, f)

            # Update time
            T_old = T_new

            # Break loop when target accuracy is met
            if target_accuracy and (accuracy >= target_accuracy):
                logging.info('Target accuracy reached.')
                break

        if reports_path:
            with open(reports_path, 'wb') as f:
                pickle.dump(self.saved_reports, f)
            logging.info('Saved reports: {}'.format(reports_path))

        network.disconnect()
        f.close()

    def async_round(self, round, T_old, network, f):
        """Run one async round for T_async"""
        logging.info("開始第%d輪訓練，時間點T_old=%f", round, T_old)
        import fl_model  # pylint: disable=import-error
        target_accuracy = self.config.fl.target_accuracy
         
        # 初始化throughput屬性，確保它始終存在
        self.throughput = 0.0
        throughputs = []
        
        # 初始化返回值的默認值
        default_accuracy = 0.0
        T_new = T_old

        # 選擇參加本輪的客戶端
        sample_clients = self.selection()
        logging.info("選擇了%d個客戶端: %s", len(sample_clients), [c.client_id for c in sample_clients])

        
        # 發送網路請求
        parsed_clients = network.parse_clients(sample_clients)       
        network.sendAsyncRequest(requestType=1, array=parsed_clients)
        logging.info("已發送網絡請求，客戶端陣列: %s", parsed_clients)
	
	#創建客戶端的映射
        id_to_client = {}
        client_finished = {}
        for client in sample_clients:
            id_to_client[client.client_id] = (client, T_old)
            client_finished[client.client_id] = False
            logging.debug("映射客戶端ID %d -> 客戶端對象", client.client_id)

        # 處理客戶端數據或模擬結束
        processed_any_client = False
        
        # Start the asynchronous updates
        processed_count = 0
        while True:
            logging.info('等待NS3回應...')
            simdata = network.readAsyncResponse()
           
            if simdata != 'end':
                #get the client/group based on the id, use map
                
                processed_any_client = True
                client_id = -1
                for key in simdata:
                    client_id = key
                    logging.info('處理客戶端 %d 的數據', client_id)

                select_client = id_to_client[client_id][0]
                select_client.delay = simdata[client_id]["endTime"]
                T_client = id_to_client[client_id][1]
                client_finished[client_id] = True
                throughputs.append(simdata[client_id]["throughput"])

                self.async_configuration([select_client], T_client)
                select_client.run(reg=True)
                T_cur = T_client + select_client.delay
                T_new = T_cur

                logging.info('Training finished on clients {} at time {} s'.format(
                    select_client, T_cur
                ))

                # Receive client updates
                reports = self.reporting([select_client])

                # Update profile and plot
                self.update_profile(reports)

                # Perform weight aggregation
                logging.info('Aggregating updates from clients {}'.format(select_client))
                staleness = select_client.delay
                updated_weights = self.aggregation(reports, staleness)

                # Load updated weights
                fl_model.load_weights(self.model, updated_weights)

                # Extract flattened weights (if applicable)
                if self.config.paths.reports:
                    self.save_reports(round, reports)

                # Save updated global model
                self.async_save_model(self.model, self.config.paths.model, T_cur)

                # Test global model accuracy
                if self.config.clients.do_test:  # Get average accuracy from client reports
                    accuracy = self.accuracy_averaging(reports)
                else:  # Test updated model on server
                    testset = self.loader.get_testset()
                    batch_size = self.config.fl.batch_size
                    testloader = fl_model.get_testloader(testset, batch_size)
                    accuracy = fl_model.test(self.model, testloader)

                self.throughput = 0
                if len(throughputs) > 0:
                    self.throughput = sum([t for t in throughputs])/len(throughputs)
                logging.info('Average accuracy: {:.2f}%\n'.format(100 * accuracy))
                self.records.async_time_graphs(T_cur, accuracy, self.throughput)

            # Return when target accuracy is met
                if target_accuracy and \
                        (self.records.get_latest_acc() >= target_accuracy):
                    logging.info('Target accuracy reached.')
                    break
            elif simdata == 'end':
                logging.info('收到NS3的ENDSIM命令')           
                # self.throughput = 0.0
                # 添加默認的精度記錄，避免 record.py 中的索引錯誤
                # 使用預設精度值 0.0（或其他合適的默認值）
                if not processed_any_client and hasattr(self, 'records'):
                    logging.info('No client data processed, adding default record')
                    self.records.async_time_graphs(T_old, default_accuracy, self.throughput)
                break

        # 輸出輪次持續時間和吞吐量
        logging.info('Round lasts {} secs, avg throughput {} kB/s'.format(
            T_new, self.throughput
        ))
        
        # 計算未完成的客戶端數
        cnt = 0
        for c in client_finished:
            if not client_finished[c]:
                cnt = cnt + 1
                f.write(str(c))
                f.write('\n')
                f.flush()
	# 即使沒有處理任何客戶端，仍然記錄輪次圖表
        self.records.async_round_graphs(round, cnt)
         
        # 使用防禦性代碼獲取最新值
        try:
            return self.records.get_latest_acc(), self.records.get_latest_t()
             
        except (IndexError, AttributeError):
            logging.warning('Failed to get latest records, returning defaults')
            return default_accuracy, T_new


    def selection(self):
        # Select devices to participate in round
        clients_per_round = self.config.clients.per_round
        select_type = self.config.clients.selection

        if select_type == 'random':
            # Select clients randomly
            sample_clients = [client for client in random.sample(
                self.clients, clients_per_round)]

        elif select_type == 'short_latency_first':
            # Select the clients with short latencies and random loss
            sample_clients = sorted(self.clients, key=lambda c:c.est_delay)
            sample_clients = sample_clients[:clients_per_round]
            print(sample_clients)

        elif select_type == 'short_latency_high_loss_first':
            # Get the non-negative losses and delays
            losses = [c.loss for c in self.clients]
            losses_norm = [l/max(losses) for l in losses]
            delays = [c.est_delay for c in self.clients]
            delays_norm = [d/max(losses) for d in delays]

            # Sort the clients by jointly consider latency and loss
            gamma = 0.2
            sorted_idx = sorted(range(len(self.clients)),
                                key=lambda i: losses_norm[i] - gamma * delays_norm[i],
                                reverse=True)
            print([losses[i] for i in sorted_idx])
            print([delays[i] for i in sorted_idx])
            sample_clients = [self.clients[i] for i in sorted_idx]
            sample_clients = sample_clients[:clients_per_round]
            print(sample_clients)

        # Create one group for each selected client to perform async updates
        #sample_groups = [Group([client]) for client in sample_clients]

        return sample_clients

    def async_configuration(self, sample_clients, download_time):
        loader_type = self.config.loader
        loading = self.config.data.loading

        if loading == 'dynamic':
            # Create shards if applicable
            if loader_type == 'shard':
                self.loader.create_shards()

        # Configure selected clients for federated learning task
        for client in sample_clients:
            if loading == 'dynamic':
                self.set_client_data(client)  # Send data partition to client

            # Extract config for client
            config = self.config

            # Continue configuration on client
            client.async_configure(config, download_time)

    def aggregation(self, reports, staleness=None):
    # 對於LEO網絡，可能需要調整staleness的處理
        if self.config.network.type == 'leo':
            # 對LEO網絡下的staleness特殊處理
            # 例如適當降低staleness懲罰
            if self.staleness_func == "polynomial":
                a = 0.3  # 降低因子，原始值為0.5
                return pow(staleness+1, -a)
        # 原始實現保持不變
        if self.staleness_func == "constant":
            return 1
        elif self.staleness_func == "polynomial":
            a = 0.5
            return pow(staleness+1, -a)
        elif self.staleness_func == "hinge":
            a, b = 10, 4
            if staleness <= b:
                return 1
            else:
                return 1 / (a * (staleness - b) + 1)

    def extract_client_weights(self, reports):
        # Extract weights from reports
        weights = [report.weights for report in reports]

        return weights

    def federated_async(self, reports, staleness):
        import fl_model  # pylint: disable=import-error

        # Extract updates from reports
        weights = self.extract_client_weights(reports)

        # Extract total number of samples
        total_samples = sum([report.num_samples for report in reports])

        # Perform weighted averaging
        new_weights = [torch.zeros(x.size())  # pylint: disable=no-member
                      for _, x in weights[0]]
        for i, update in enumerate(weights):
            num_samples = reports[i].num_samples
            for j, (_, weight) in enumerate(update):
                # Use weighted average by number of samples
                new_weights[j] += weight * (num_samples / total_samples)

        # Extract baseline model weights - latest model
        baseline_weights = fl_model.extract_weights(self.model)

        # Calculate the staleness-aware weights
        alpha_t = self.alpha * self.staleness(staleness)
        logging.info('{} staleness: {} alpha_t: {}'.format(
            self.staleness_func, staleness, alpha_t
        ))

        # Load updated weights into model
        updated_weights = []
        for i, (name, weight) in enumerate(baseline_weights):
            updated_weights.append(
                (name, (1 - alpha_t) * weight + alpha_t * new_weights[i])
            )

        return updated_weights

    def staleness(self, staleness):
        if self.staleness_func == "constant":
            return 1
        elif self.staleness_func == "polynomial":
            a = 0.5
            return pow(staleness+1, -a)
        elif self.staleness_func == "hinge":
            a, b = 10, 4
            if staleness <= b:
                return 1
            else:
                return 1 / (a * (staleness - b) + 1)

    def async_save_model(self, model, path, download_time):
        path += '/global_' + '{}'.format(download_time)
        torch.save(model.state_dict(), path)
        logging.info('Saved global model: {}'.format(path))

    def rm_old_models(self, path, cur_time):
        for filename in os.listdir(path):
            try:
                model_time = float(filename.split('_')[1])
                if model_time < cur_time:
                    os.remove(os.path.join(path, filename))
                    logging.info('Remove model {}'.format(filename))
            except Exception as e:
                logging.debug(e)
                continue

    def update_profile(self, reports):
        for report in reports:
            self.profile.update(report.client_id, report.loss, report.delay,
                                self.flatten_weights(report.weights))
