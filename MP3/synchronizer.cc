#include <iostream>
#include <string>
#include <fstream>
#include <thread>
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

pair<int, int> synch();

std::unique_ptr<CoordService::Stub> cstub = nullptr;

std::unique_ptr<SynchService::Stub> fsStub1 = nullptr;

std::unique_ptr<SynchService::Stub> fsStub2 = nullptr;

int id = 1;

class SynchServiceImpl final : public SynchService::Service {
  
    Status Newuser(ServerContext* context, const Update* update, Response* response) override {
		pair<int, int> ports = synch();
		string filename = "cache/" + std::to_string(ports.first) + "/addusers.txt";
		ofstream ofs(filename);
		for(int i = 0; i < update->arguments_size(); i++)
		{
			if(i != update->arguments_size() - 1)
				ofs << update->arguments(i) << "\n";
			else
				ofs << update->arguments(i);
		}
		ofs.close();
        return Status::OK;
    }

	Status Follow(ServerContext* context, const Update* update, Response* response) override {
		return Status::OK;
	}

	Status Timeline(ServerContext* context, const Update* update, Response* response) override {
		return Status::OK;
	}
};

void checkNewUsers(pair<int, int> ports) {
	string filename = "cache/" + std::to_string(ports.first) + "/newusers.txt";
	ifstream ifs(filename);
	if(ifs.is_open())
	{
		Update up;
		string username = "";
		while(!ifs.eof())
		{
			getline(ifs, username);
			if(username.size() == 0)
				break;
			up.add_arguments(username);
			username = "";
		}
		ifs.close();

		int rmVal = remove(filename.c_str());

		if(ports.second != -1)
		{
			string filename2 = "cache/" + std::to_string(ports.second) + "/newusers.txt";
			rmVal = remove(filename2.c_str());
		}

		ClientContext cc1;
		ClientContext cc2;
		Response resp1;
		Response resp2;
		Status status1 = fsStub1->Newuser(&cc1, up, &resp1);
		if(!status1.ok())
		{
			cout << "FS1 grpc error" << endl;
		}
		Status status2 = fsStub2->Newuser(&cc2, up, &resp2);
		if(!status2.ok())
		{
			cout << "FS2 grpc error" << endl;
		}
	}
	else
	{
		cout << "No new users detected." << endl;
		return;
	}
}

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

void run() {
	while(true)
	{
		sleep(30);
		pair<int, int> ports = synch();
		thread newUserThread(checkNewUsers, ports);
		newUserThread.detach();
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

void setup(string coordAddress, string coordPort, string port) {
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

	login_info = rep.ipaddress() + ":" + std::to_string(rep.port());
    fsStub1 = std::unique_ptr<SynchService::Stub>(SynchService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
	
	cout << "FS " << id << " connected to FS at " << login_info << endl;
	
	login_info = rep.address_two() + ":" + std::to_string(rep.port_two());
    fsStub2 = std::unique_ptr<SynchService::Stub>(SynchService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

	cout << "FS " << id << " connected to FS at " << login_info << endl;
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

	thread runnerThread(run);
	runnerThread.detach();

	RunServer(port);

	return 0;
}