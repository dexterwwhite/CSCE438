#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coord.grpc.pb.h"
#include "synch.grpc.pb.h"

using std::cout;
using std::endl;
using std::string;
using std::ifstream;
using std::ofstream;
using std::thread;
using std::pair;
using std::mutex;
using std::unique_lock;
using std::vector;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using coordinator::CoordService;
using coordinator::Pulse;
using coordinator::Request;
using coordinator::Reply;
using synchronizer::SynchService;
using synchronizer::Update;
using synchronizer::Response;

//Declaration of synch() function
pair<int, int> synch();

//Stub created for communication with Coordinator
std::unique_ptr<CoordService::Stub> cstub = nullptr;

//Stubs for the other 2 follower synchronizers
std::unique_ptr<SynchService::Stub> fsStub1 = nullptr;
std::unique_ptr<SynchService::Stub> fsStub2 = nullptr;

//Stub for communication with self
std::unique_ptr<SynchService::Stub> selfStub = nullptr;

//Global variables
int id = 1;
mutex mtx;

class SynchServiceImpl final : public SynchService::Service {

	//Service for communicating changes to other server clusters
	//Only used for follower synchronizer to follower synchronizer communication
	Status Change(ServerContext* context, const Update* update, Response* response) override {
		//Checks for master server port from coordinator
		pair<int, int> ports = synch();
		string filename = "cache/" + std::to_string(ports.first) + "/addchanges.txt";
		{
			unique_lock<mutex> followLock(mtx);
			ifstream ifs(filename);
			if(ifs.is_open())
			{
				//If "addchanges.txt" already exists, adds each line to a vector
				vector<string> prevLines;
				string line = "";
				while(!ifs.eof())
				{
					getline(ifs, line);
					if(line == "")
						break;
					prevLines.push_back(line);
					line = "";
				}
				ifs.close();

				//Merges previous "addchanges.txt" data with data from update parameter, sorting by time
				int vecIndex = 0;
				int argIndex = 0;
				ofstream ofs(filename);
				while(argIndex < update->arguments_size())
				{
					if(vecIndex >= prevLines.size())
					{
						while(argIndex < update->arguments_size())
						{
							if(argIndex == update->arguments_size() - 1)
							{
								ofs << update->arguments(argIndex);
							}
							else
								ofs << update->arguments(argIndex) << "\n";
							argIndex++;
						}
						break;
					}
					vecIndex++;
					string vecTime = prevLines.at(vecIndex);
					argIndex++;
					string argTime = update->arguments(argIndex);
					if(argTime > vecTime)
					{
						ofs << prevLines.at(vecIndex - 1) << "\n";
						ofs << prevLines.at(vecIndex) << "\n";
						ofs << prevLines.at(vecIndex + 1) << "\n";
						vecIndex += 2;
						argIndex--;
					}
					else
					{
						ofs << update->arguments(argIndex - 1) << "\n";
						ofs << update->arguments(argIndex) << "\n";
						ofs << update->arguments(argIndex + 1) << "\n";\
						argIndex += 2;
						vecIndex--;
					}
				}
				if(vecIndex != prevLines.size())
				{
					while(vecIndex < prevLines.size())
					{
						if(vecIndex == prevLines.size() - 1)
						{
							ofs << prevLines.at(vecIndex);
						}
						else
							ofs << prevLines.at(vecIndex) << "\n";
						vecIndex++;
					}
				}
				ofs.close();
			}
			else
			{
				//If there was not a previous "addchanges.txt", creates one and fills in data
				ofstream ofs(filename);
				for(int i = 0; i < update->arguments_size(); i++)
				{
					if(i != update->arguments_size() - 1)
						ofs << update->arguments(i) << "\n";
					else
						ofs << update->arguments(i);
				}
				ofs.close();
			}
		}
		return Status::OK;
	}
};

//Checks the "recentchanges.txt" file and sends any data included to synchronizers in other server clusters
void checkChanges(pair<int, int> ports) {
	string filename = "cache/" + std::to_string(ports.first) + "/recentchanges.txt";
	ifstream ifs(filename);
	if(ifs.is_open())
	{
		//Reads in all data from "recentchanges.txt"
		Update up;
		string line = "";
		while(!ifs.eof())
		{
			getline(ifs, line);
			if(line == "")
				break;
			up.add_arguments(line);
			line = "";
		}
		ifs.close();

		//Deletes file, allows synchronizer to know it is up to date
		int rmVal = remove(filename.c_str());

		//If both the slave and master are active, deletes "recentchanges.txt" from both servers
		if(ports.second != -1)
		{
			string filename2 = "cache/" + std::to_string(ports.second) + "/recentchanges.txt";
			rmVal = remove(filename2.c_str());
		}

		//Shares changes with other follower synchronizers
		ClientContext cc1;
		ClientContext cc2;
		Response resp1;
		Response resp2;
		Status status1 = fsStub1->Change(&cc1, up, &resp1);
		if(!status1.ok())
		{
			cout << "FS1 grpc error" << endl;
		}
		Status status2 = fsStub2->Change(&cc2, up, &resp2);
		if(!status2.ok())
		{
			cout << "FS2 grpc error" << endl;
		}

		//Shares changes with self
		ClientContext cc3;
		Response resp3;
		Status status3 = selfStub->Change(&cc3, up, &resp3);
	}
}

//Allows synchronizer to check the current master server for the cluster
//If both servers are currently active, the first value is master port and second value is slave port
pair<int, int> synch() {
	ClientContext cc;
	Request req;
	req.set_id(id);
	Reply rep;
	Status status = cstub->Synch(&cc, req, &rep);
	if(!status.ok())
	{
		cout << "GRPC Error. FS terminating" << endl;
		exit(1);
	}
	pair<int, int> ports(rep.port(), rep.port_two());
	return ports;
}

//Sets up a stub for communication with itself
void selfSetUp(string address, string port) {
	sleep(3);
	string self = address + ":" + port;
	selfStub = std::unique_ptr<SynchService::Stub>(SynchService::NewStub(grpc::CreateChannel(self, grpc::InsecureChannelCredentials())));
}

//Synchronizer runs every 30 seconds
//Checks current master server with synch() before checking for changes
void run() {
	while(true)
	{
		sleep(30);
		pair<int, int> ports = synch();
		thread changes(checkChanges, ports);
		changes.detach();
	}
}

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SynchServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Synchronizer listening on " << server_address << std::endl;

  server->Wait();
}

//Synchronizer sets up a channel with coordinator and other 2 synchronizers
void setup(string coordAddress, string coordPort, string port) {
	//connects to coordinator
	string login_info = coordAddress + ":" + coordPort;
    cstub = std::unique_ptr<CoordService::Stub>(CoordService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
	ClientContext cc;
	coordinator::Request req;
	req.set_type("synchronizer");
	req.set_id(id);
	req.add_arguments(port);
	coordinator::Reply rep;
	Status status = cstub->Connect(&cc, req, &rep);
    if(!status.ok())
    {
        exit(1);
    }

	//Connects to other follower synchronizers
	login_info = rep.ipaddress() + ":" + std::to_string(rep.port());
    fsStub1 = std::unique_ptr<SynchService::Stub>(SynchService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
	
	login_info = rep.address_two() + ":" + std::to_string(rep.port_two());
    fsStub2 = std::unique_ptr<SynchService::Stub>(SynchService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
}

int main(int argc, char** argv) {
	string coordAddress = "127.0.0.1";
	string coordPort = "1234";
	string port = "1237";
	int opt = 0;
	while ((opt = getopt(argc, argv, "h:c:p:i:")) != -1){
		switch(opt) {
		case 'h':
			coordAddress = optarg;
			break;
		case 'c':
			coordPort = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'i':
			id = atoi(optarg);
			break;
		default:
		std::cerr << "Invalid Command Line Argument\n";
		}
	}
	setup(coordAddress, coordPort, port);

	thread setupThread(selfSetUp, "127.0.0.1", port);
	setupThread.detach();

	thread runnerThread(run);
	runnerThread.detach();

	RunServer(port);

	return 0;
}