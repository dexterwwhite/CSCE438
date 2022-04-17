#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coord.grpc.pb.h"

using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::mutex;
using std::unique_lock;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using coordinator::CoordService;
using coordinator::Request;
using coordinator::Reply;

class Cluster {
    private:
        int clusterNum;
        string ipAddress;
        int masterPort;
        int slavePort;
        int synchPort;
        vector<int> clients;
    public:
        Cluster() {}
        void setClusterNum(int num) { clusterNum = num; }
        int getClusterNum() { return clusterNum; }
        void setIP(string ip) { ipAddress = ip; }
        string getIP() { return ipAddress; }
        void setMasterPort(int port) { masterPort = port; }
        int getMasterPort() { return masterPort; }
        void setSlavePort(int port) { slavePort = port; }
        int getSlavePort() { return slavePort; }
        void setSynchPort(int port) { synchPort = port; }
        int getSynchPort() { return synchPort; }
        void addClient(int id) { clients.push_back(id); }
};

vector<Cluster> serverClusters;
mutex mtx;

class CoordServiceImpl final : public CoordService::Service {
  
    Status Connect(ServerContext* context, const Request* request, Reply* reply) override {

        if(request->type() == "client")
        {
            int id = request->id();

            //ID - 1 means Client #1 will be placed into server cluster 1 (index 0 of the vector of clusters)
            int cluster = (id - 1) % 3;
            {
                unique_lock<mutex> connectLock(mtx);
                serverClusters.at(cluster).addClient(id);
                reply->set_ipaddress(serverClusters.at(cluster).getIP());
                reply->set_port(serverClusters.at(cluster).getMasterPort());
            }
            cout << "ID: " << id << endl;
            cout << "Client will be placed in server cluster " << cluster + 1 << endl;
        }
        else if(request->type() == "server")
        {
            int id = request->id();
            int cluster = id - 1;
            bool isMaster = false;
            if(request->arguments(0) == "master")
                isMaster = true;

            int port = stoi(request->arguments(2));
            {
                unique_lock<mutex> connectLock(mtx);
                if(isMaster)
                {
                    serverClusters.at(cluster).setIP(request->arguments(1));
                    serverClusters.at(cluster).setMasterPort(port);
                }
                else
                    serverClusters.at(cluster).setSlavePort(port);
            }
            cout << "Server placed in cluster " << id << endl;
            cout << "Server is of type " << request->arguments(0) << endl;
            cout << "Server port is " << port << endl;
        }
        return Status::OK;
    }
};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  CoordServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Coordinator listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt) {
        case 'p':
            port = optarg;break;
        default:
        std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Cluster c1;
    c1.setIP("0.0.0.0");
    c1.setMasterPort(1234);
    Cluster c2;
    c2.setIP("0.0.0.0");
    c2.setMasterPort(1234);
    Cluster c3;
    c3.setIP("0.0.0.0");
    c3.setMasterPort(1234);
    serverClusters.push_back(c1);
    serverClusters.push_back(c2);
    serverClusters.push_back(c3);

    RunServer(port);

    return 0;
}