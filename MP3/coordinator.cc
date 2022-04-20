/**

    NEED TO DO:
    Implement Heartbeat() -- DONE?!
    Implement follow synchronizer Connect()

*/

#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
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
using std::thread;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using coordinator::CoordService;
using coordinator::Request;
using coordinator::Reply;
using coordinator::Pulse;

class Server {
    private:
        bool isMaster;
        int port;
        bool checkedIn;
        bool warning;
        bool active;

    public:
        Server(bool master, int port_no) : isMaster(master), port(port_no)
        {
            checkedIn = true;
            warning = false;
            active = true;
        }
        int getPort() { return port; }
        void setMasterStatus() { isMaster = true; }
        void notMaster() { isMaster = false; }
        bool master() { return isMaster; }
        void checkIn() { checkedIn = true; }
        void checkOut() { checkedIn = false; }
        bool check() { return checkedIn; }
        void setWarning() { warning = true; }
        void rmWarning() { warning = false; }
        bool warningStatus() { return warning; }
        void activate() { active = true; }
        void deactivate() { active = false; }
        bool isActive() { return active; }
};

class Cluster {
    private:
        int clusterNum;
        string ipAddress;
        int synchPort;
        vector<int> clients;
        bool swap;
        Server* master;
        Server* slave;

    public:
        Cluster(int num) : clusterNum(num)
        {
            ipAddress = "na";
            synchPort = 0;
            swap = false;
            master = nullptr;
            slave = nullptr;
        }
        //Routing Functions
        int getClusterNum() { return clusterNum; }
        void setIP(string ip) { ipAddress = ip; }
        string getIP() { return ipAddress; }
        void setMaster(Server* mas) {
            master = mas;
        }
        Server* getMaster() { return master; }
        void setSlave(Server* sla) {
            slave = sla;
        }
        Server* getSlave() { return slave; }
        void setSynchPort(int port) { synchPort = port; }
        int getSynchPort() { return synchPort; }
        void addClient(int id) { clients.push_back(id); }
        void promote() {
            Server* temp = master;
            master = slave;
            slave = temp;
            master->setMasterStatus();
            slave->notMaster();
            slave->deactivate();
            temp = nullptr;
        }

        //Heartbeat Functions
        void swapOn() { swap = true; }
        void swapOff() { swap = false; }
        bool swapStatus() { return swap; }
};

vector<Cluster> serverClusters;
mutex mtx;

void heartBeatThread(int clusterID, Server* server);

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
                reply->set_port(serverClusters.at(cluster).getMaster()->getPort());
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
                    Server* mServer = new Server(true, port);
                    serverClusters.at(cluster).setIP(request->arguments(1));
                    serverClusters.at(cluster).setMaster(mServer);
                }
                else
                {
                    Server* sServer = new Server(false, port);
                    serverClusters.at(cluster).setSlave(sServer);
                }
            }
            cout << "Server placed in cluster " << id << endl;
            cout << "Server is of type " << request->arguments(0) << endl;
            cout << "Server port is " << port << endl;
        }
        else if(request->type() == "master")
        {
            int id = request->id();
            int cluster = id - 1;
            while(serverClusters.at(cluster).getSlave() == nullptr)
            {
                sleep(1);
            }
            {
                unique_lock<mutex> connectLock(mtx);
                reply->set_ipaddress(serverClusters.at(cluster).getIP());
                reply->set_port(serverClusters.at(cluster).getSlave()->getPort());
            }
        }
        else if(request->type() == "synchronizer")
        {
            int id = request->id();
            int cluster = id - 1;
            int port = stoi(request->arguments(0));
            {
                unique_lock<mutex> connectLock(mtx);
                serverClusters.at(cluster).setSynchPort(port);
            }
            int count = 0;
            for(int i = 0; i < 3; i++)
            {
                if(i == cluster)
                {
                    continue;
                }
                while(i >= serverClusters.size() || serverClusters.at(i).getSynchPort() == 0 || serverClusters.at(i).getIP() == "na")
                {
                    sleep(.5);
                }
                if(count == 0)
                {
                    unique_lock<mutex> connectLock(mtx);
                    reply->set_ipaddress(serverClusters.at(i).getIP());
                    reply->set_port(serverClusters.at(i).getSynchPort());
                    count++;
                }
                else
                {
                    unique_lock<mutex> connectLock(mtx);
                    reply->set_address_two(serverClusters.at(i).getIP());
                    reply->set_port_two(serverClusters.at(i).getSynchPort());
                }
            }
        }
        return Status::OK;
    }

    Status Heartbeat(ServerContext* context, ServerReaderWriter<Pulse, Pulse>* stream) override {
        Pulse pulse;
        int clusterID;
        bool isMaster = false;
        stream->Read(&pulse);
        clusterID = pulse.id() - 1;
        sleep(2);
        if(pulse.type() == "master")
        {
            unique_lock<mutex> threadLock(mtx);
            isMaster = true;
            thread listenerThread(heartBeatThread, clusterID, serverClusters.at(clusterID).getMaster());
            listenerThread.detach();
        }
        else
        {
            unique_lock<mutex> threadLock(mtx);
            thread listenerThread(heartBeatThread, clusterID, serverClusters.at(clusterID).getSlave());
            listenerThread.detach();
        }

        while(stream->Read(&pulse))
        {
            if(isMaster)
            {
                unique_lock<mutex> beatLock(mtx);
                serverClusters.at(clusterID).getMaster()->checkIn();
                serverClusters.at(clusterID).getMaster()->rmWarning();
            }
            else
            {
                unique_lock<mutex> beatLock(mtx);
                serverClusters.at(clusterID).getSlave()->checkIn();
                serverClusters.at(clusterID).getSlave()->rmWarning();
                if(serverClusters.at(clusterID).swapStatus())
                {
                    Pulse swapMsg;
                    swapMsg.add_arguments("swap");
                    serverClusters.at(clusterID).promote();
                    serverClusters.at(clusterID).swapOff();
                    isMaster = true;
                    stream->Write(swapMsg);
                }
            }
        }
    
        return Status::OK;
    }

    Status Synch(ServerContext* context, const Request* request, Reply* reply) override {
        int id = request->id();
        int cluster = id - 1;
        unique_lock<mutex> synchLock(mtx);
        reply->set_port(serverClusters.at(cluster).getMaster()->getPort());
        if(serverClusters.at(cluster).getSlave()->isActive())
        {
            reply->set_port_two(serverClusters.at(cluster).getSlave()->getPort());
        }
        else
        {
            reply->set_port_two(-1);
        }
        return Status::OK;
    }
};

void heartBeatThread(int clusterID, Server* server) {
    while(true)
    {
        {
            unique_lock<mutex> hbtLock(mtx);
            if(server->check())
            {
                //cout << "Server is alive!" << endl;
                server->checkOut();
            }
            else
            {
                if(server->warningStatus())
                {
                    cout << "Server is determined dead" << endl;
                    if(server->master())
                    {
                        serverClusters.at(clusterID).swapOn();
                    }
                    break;
                }
                else
                {
                    cout << "Server has not responded for 1 heartbeat!" << endl;
                    server->setWarning();
                }
            }
        }
        sleep(10);
    }
}

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  CoordServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
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

    Cluster c1(1);
    Cluster c2(2);
    Cluster c3(3);
    serverClusters.push_back(c1);
    serverClusters.push_back(c2);
    serverClusters.push_back(c3);

    RunServer(port);

    return 0;
}