#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coord.grpc.pb.h"

using std::cout;
using std::endl;
using std::vector;
using std::string;
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

class CoordServiceImpl final : public CoordService::Service {
  
    Status Connect(ServerContext* context, const Request* request, Reply* reply) override {
        int id = request->id();
        cout << "ID: " << id << endl;
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
    Cluster c2;
    Cluster c3;
    serverClusters.push_back(c1);
    serverClusters.push_back(c2);
    serverClusters.push_back(c3);

    RunServer(port);

    return 0;
}