/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>

#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"
#include "coord.grpc.pb.h"

using std::cout;
using std::endl;
using std::string;
using std::thread;
using std::mutex;
using std::unique_lock;
using std::vector;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using coordinator::CoordService;
using coordinator::Pulse;
using grpc::ClientContext;
using grpc::ClientReaderWriter;

//Same Client struct from sample code, added updateReady boolean
//updateReady signifies whether client timeline has received updates
struct Client {
  std::string username;
  bool connected = true;
  bool updateReady = false;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client> client_db;

//Client stub for communication with coordinator
std::unique_ptr<CoordService::Stub> cstub;

//SNS Service client stubs for communicating with slave and with self
std::unique_ptr<SNSService::Stub> slaveStub = nullptr;
std::unique_ptr<SNSService::Stub> selfStub = nullptr;

//Global vector determining whether server is the master
bool master;

//File header for ease of use when working with files
//Follows form "cache/<port#>"
string fileHeader;

mutex mtx;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

//After client enters timeline mode, every 3 seconds this function checks whether changes
//have been made to it's timeline. If so, sends the latest 20 posts from the timeline to
//the client
void updateTimeline(ServerReaderWriter<Message, Message>* stream, string username) {
	while(true)
	{
		sleep(3);
		{
			unique_lock<mutex> utlLock(mtx);
			int userIndex = find_user(username);
			Client* client = &client_db.at(userIndex);
			//If client has updates to its timeline, enters conditional
			if(client->updateReady)
			{
				//Enter timeline is read in
				string filename = fileHeader + "/" + username + "/timeline.txt";
				std::ifstream ifs(filename);
				vector<string> posts;
				vector<string> times;
				string line = "";
				while(!ifs.eof())
				{
					getline(ifs, line);
					if(line == "")
						break;
					times.push_back(line);
					getline(ifs, line);
					posts.push_back(line);
					
				}

				//If timeline is >20 posts, only sends the latest 20
				int index = 0;
				if(posts.size() > 20)
					index = posts.size() - 20;
				while(index < posts.size())
				{
					Message message;
					string user = "";
					int i;
					for(i = 0; i < posts.at(index).length(); i++)
					{
						if(posts.at(index).at(i) == ' ')
							break;
						user += posts.at(index).at(i);
					}

					//Formats post before sending to client
					string post = posts.at(index);
					string body = post.substr(i + 1, post.length() - i - 1);
					message.set_msg("(" + times.at(index) + ") " + user + ": " + body);
					stream->Write(message);
					index++;
				}

				//Sets client "update" boolean back to false
				client->updateReady = false;
			}
		}
	}
}

class SNSServiceImpl final : public SNSService::Service {
  
  //Unchanged Service
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it != user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    return Status::OK;
  }

	Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
		//If this server is the master, sends Follow to slave with stub
		if(master && slaveStub != nullptr)
		{
			ClientContext cc;
			Reply slaveReply;
			slaveStub->Follow(&cc, *request, &slaveReply);
		}
		std::string username1 = request->username();
		std::string username2 = request->arguments(0);
		int join_index = find_user(username2);
		if(join_index < 0 || username1 == username2)
			reply->set_msg("unkown user name");
		else
		{
			Client *user1 = &client_db[find_user(username1)];
			Client *user2 = &client_db[join_index];
			if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end())
			{
				reply->set_msg("you have already joined");
				return Status::OK;
			}
			user1->client_following.push_back(user2);
			string fileName1 = fileHeader + "/" + username1 + "/following.txt";
			std::ofstream ofs(fileName1, std::ofstream::app);
			ofs << username2 << "\n";
			ofs.close();

			//"Synch" argument is used to determine if this was an update from the synchronizer
			//If it was not, adds this follow request to the "recentchanges.txt" file to be read in by synchronizer
			if(request->arguments_size() == 1 || request->arguments(1) != "synch")
			{
				google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
				timestamp->set_seconds(time(NULL));
				timestamp->set_nanos(0);
				string time = google::protobuf::util::TimeUtil::ToString(*timestamp);
				{
					unique_lock<mutex> followLock(mtx);
					string changeFilename = fileHeader + "/recentchanges.txt";
					ofs.open(changeFilename, std::ofstream::app);
					ofs << "FOLLOW\n";
					ofs << time << "\n";
					ofs << username1 << " " << username2 << "\n";
					ofs.close();
				}
			}

			user2->client_followers.push_back(user1);
			string fileName2 = fileHeader + "/" + username2 + "/followers.txt";
			ofs.open(fileName2, std::ofstream::app);
			ofs << username1 << "\n";
			ofs.close();

			reply->set_msg("Follow Successful");
		}
		return Status::OK; 
	}

	//Unused Service
  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
	return Status::OK;
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("unknown follower username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("you are not follower");
        return Status::OK;
      }
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      reply->set_msg("UnFollow Successful");
    }
    return Status::OK;
  }
  
	Status Login(ServerContext* context, const Request* request, Reply* reply) override {
		//If server is master, sends login request to the slave as well
		if(master && slaveStub != nullptr)
		{
			ClientContext cc;
			Reply slaveReply;
			slaveStub->Login(&cc, *request, &slaveReply);
		}
		Client c;
		std::string username = request->username();
		int user_index = find_user(username);
		if(user_index < 0)
		{
			c.username = username;
			client_db.push_back(c);
			reply->set_msg("Login Successful!");

			//Make their directory and other files
			string dirName = fileHeader + "/" + username;
			mkdir(dirName.c_str(), 0777);
			string firstFile = dirName + "/following.txt";
			std::ofstream ofs(firstFile);
			ofs.close();
			string secondFile = dirName + "/followers.txt";
			std::ofstream ofs2(secondFile);
			ofs2.close();

			string thirdFile = dirName + "/timeline.txt";
			ofs.open(thirdFile);
			ofs.close();

			//If this update was not sent over by the synchronizer, adds this as a new user
			//to the "recentchanges.txt" file.
			if(request->arguments_size() == 0 || request->arguments(0) != "synch")
			{
				google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
				timestamp->set_seconds(time(NULL));
				timestamp->set_nanos(0);
				string time = google::protobuf::util::TimeUtil::ToString(*timestamp);

				unique_lock<mutex> loginLock(mtx);
				string newUser = fileHeader + "/recentchanges.txt";
				ofs.open(newUser, std::ofstream::app);
				ofs << "NEWUSER" << "\n";
				ofs << time << "\n";
				ofs << username << "\n";
				ofs.close();
			}

		}
		else
		{ 
			Client *user = &client_db[user_index];
			if(user->connected)
				reply->set_msg("Invalid Username");
			else
			{
				std::string msg = "Welcome Back " + user->username;
				reply->set_msg(msg);
				user->connected = true;
			}
		}
		return Status::OK;
	}

	Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
		std::unique_ptr<ClientReaderWriter<Message, Message>> slaveStream = nullptr;
		ClientContext cc;

		//If server is master, sets up a stream to send any messages to slave as well
		if(master && slaveStub != nullptr)
		{
			slaveStream = slaveStub->Timeline(&cc);
		}

		bool first = true;
		Message message;
		Client *c;
		while(stream->Read(&message)) 
		{
			if(master && slaveStream != nullptr)
			{
				Message m1 = message;
				slaveStream->Write(m1);
			}
			std::string username = message.username();
			int user_index = find_user(username);
			c = &client_db[user_index];

			//Separate thread for waiting for updates to client timeline
			if(master && first)
			{
				thread updateTLThread(updateTimeline, stream, username);
				updateTLThread.detach();
				first = false;
			}
		
			//Write the current message to "timeline.txt"
			std::string filename = fileHeader + "/" + username + "/timeline.txt";
			std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
			google::protobuf::Timestamp temptime = message.timestamp();
			std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
			std::string fileinput = time+" :: "+message.username()+":"+message.msg();
			//"Set Stream" is the default message from the client to initialize the stream
			if(message.msg() != "Set Stream")
			{
				//user_file << fileinput;
				cout << "";
			}
			//If message = "Set Stream", print the first 20 chats from the people you follow
			else
			{
				if(c->stream==0)
					c->stream = stream;
				std::string line;
				std::vector<std::string> newest_twenty;
				string fname = fileHeader + "/" + username + "/timeline.txt";
				std::ifstream in(fname);
				int count = 0;
				//Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
				while(!in.eof())
				{
					getline(in, line);
					if(line == "")
						break;
					string post = "(" + line + ") ";
					getline(in, line);
					post += line;
					newest_twenty.push_back(post);
					line = "";
				}
				Message new_msg; 
				in.close();

				int index = 0;
				if(newest_twenty.size() > 20)
					index = newest_twenty.size() - 20;
				//Send the newest messages to the client to be displayed
				for(int i = index; i < newest_twenty.size(); i++)
				{
					new_msg.set_msg(newest_twenty[i]);
					stream->Write(new_msg);
				}    
				continue;
			}
			//Add message to "recentchanges.txt"
			{
				unique_lock<mutex> timelineLock(mtx);
				string tlFile = fileHeader + "/recentchanges.txt";
				std::ofstream ofsTL(tlFile, std::ofstream::app);
				ofsTL << "TIMELINE" << "\n";
				ofsTL << time << "\n";
				ofsTL << username << " " <<message.msg();
				ofsTL.close();
			}
		}
		//If the client disconnected from Chat Mode, set connected to false
		c->connected = false;
		return Status::OK;
	}

	//This service is solely used for communicating timeline updates between master and slave
	Status UpdateTL(ServerContext* context, const Request* request, Reply* reply) override {
		string time = request->arguments(0);
		string user = request->arguments(1);
		string post = request->arguments(2);

		//Writes timelime post to client's own timeline file
		unique_lock<mutex> synchLock(mtx);
		string ofsFile = fileHeader + "/" + user + "/timeline.txt";
		std::ofstream ofs(ofsFile, std::ofstream::app);
		ofs << time << "\n";
		ofs << user << " " << post << "\n";
		ofs.close();

		//Writes timeline post to client's followers' timeline files
		int userIndex = find_user(user);
		Client* origClient = &client_db.at(userIndex);
		for(int i = 0; i < client_db.at(userIndex).client_followers.size(); i++)
		{
			Client* temp = client_db.at(userIndex).client_followers.at(i);
			string followerfile = fileHeader + "/" + temp->username + "/timeline.txt";
			ofs.open(followerfile, std::ofstream::app);
			ofs << time << "\n";
			ofs << user << " " << post << "\n";
			ofs.close();
		}
		return Status::OK;
	}

};

//Since synchronizer does not directly communicate with server server searches for
//synchronizer "addchanges.txt" files every 2 seconds
void synchronizeChanges() {
	while(true)
	{
		sleep(2);
		string filename = fileHeader + "/addchanges.txt";
		std::ifstream ifs(filename);
		if(ifs.is_open())
		{
			string line = "";
			while(!ifs.eof())
			{
				getline(ifs, line);
				if(line == "NEWUSER")
				{
					//Reads in timestamp from file
					getline(ifs, line);

					//Reads in username from file
					getline(ifs, line);

					//Sends login service request to SELF
					ClientContext cc;
					Request req;
					req.set_username(line);
					req.add_arguments("synch");
					Reply rep;
					selfStub->Login(&cc, req, &rep);
				}
				else if(line == "FOLLOW")
				{
					string time;
					getline(ifs, time);
					string users;
					getline(ifs, users);

					//Parses for following username and followed username
					string user1 = "";
					int i;
					for(i = 0; i < users.length(); i++)
					{
						if(users.at(i) == ' ')
							break;
						user1 += users.at(i);
					}

					string user2 = "";
					for(i = i + 1; i < users.length(); i++)
					{
						if(users.at(i) == '\n')
							break;
						user2 += users.at(i);
					}

					//Sends follow service request to SELF
					ClientContext cc;
					Request req;
					req.set_username(user1);
					req.add_arguments(user2);
					req.add_arguments("synch");
					Reply rep;
					selfStub->Follow(&cc, req, &rep);
				}
				else if(line == "TIMELINE")
				{
					string time;
					getline(ifs, time);
					getline(ifs, line);

					//Parses for username
					string user = "";
					int i;
					for(i = 0; i < line.length(); i++)
					{
						if(line.at(i) == ' ')
							break;
						user += line.at(i);
					}

					//Parses for post
					string post = "";
					for(i = i + 1; i < line.length(); i++)
					{
						if(line.at(i) == '\n')
							break;
						post += line.at(i);
					}

					{
						if(master && slaveStub != nullptr)
						{
							//If server is master, also sends timeline post to slave
							ClientContext ccs;
							Request tlreq;
							tlreq.add_arguments(time);
							tlreq.add_arguments(user);
							tlreq.add_arguments(post);
							Reply tlrep;
							slaveStub->UpdateTL(&ccs, tlreq, &tlrep);
						}

						//Adds post to user's own timeline
						unique_lock<mutex> synchLock(mtx);
						string ofsFile = fileHeader + "/" + user + "/timeline.txt";
						std::ofstream ofs(ofsFile, std::ofstream::app);
						ofs << time << "\n";
						ofs << user << " " << post << "\n";
						ofs.close();

						//Adds posts to user's followers' timelines and updates updateReady boolean for all users
						int userIndex = find_user(user);
						Client* origClient = &client_db.at(userIndex);
						origClient->updateReady = true;
						for(int i = 0; i < client_db.at(userIndex).client_followers.size(); i++)
						{
							Client* temp = client_db.at(userIndex).client_followers.at(i);
							string followerfile = fileHeader + "/" + temp->username + "/timeline.txt";
							ofs.open(followerfile, std::ofstream::app);
							ofs << time << "\n";
							ofs << user << " " << post << "\n";
							ofs.close();
							temp->updateReady = true;
						}
					}
				}
			}
			ifs.close();
			remove(filename.c_str());
		}
	}
}

//Sets up channel stub with self
void selfSetUp(string address, string port) {
	string self = address + ":" + port;
	selfStub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(self, grpc::InsecureChannelCredentials())));
}

//Creates a separate thread to check for synchronizer changes
void synchronize(string address, string port) {
	selfSetUp(address, port);

	thread changes(synchronizeChanges);
	changes.detach();
}

//Listens for a response from coordinator
//If coordinator sends a heartbeat response, this can only mean the master has died
void heartbeatListen(std::shared_ptr<ClientReaderWriter<Pulse, Pulse>> stream) {
	Pulse pulse;
	while(stream->Read(&pulse))
	{
		if(pulse.arguments_size() > 0)
		{
			if(pulse.arguments(0) == "swap")
			{
				//Server has been promoted to master
				cout << "Switching to master!" << endl;
				master = true;
			}
		}
	}
}

//Server maintains a heartbeat with coordinator
void heartbeat(int id) {
	ClientContext context;

	//Sets up initial connection with coordinator
    std::shared_ptr<ClientReaderWriter<Pulse, Pulse>> stream(cstub->Heartbeat(&context));
	Pulse pulse;
	pulse.set_id(id);
	if(master)
	{
		pulse.set_type("master");
	}
	else
	{
		pulse.set_type("slave");
	}
	stream->Write(pulse);

	//Creates separate thread for listening
	thread listener(heartbeatListen, stream);
	listener.detach();
	while(true)
	{
		//Every 10 seconds sends a heartbeat pulse
		sleep(10);
		stream->Write(pulse);
	}
}

//Function for master server to locate ip address and port of slave server
//Sets up a stub to communicate with the slave
void locateSlave(int id) {
	ClientContext context;
	coordinator::Request request;
	request.set_type("master");
	request.set_id(id);
	coordinator::Reply reply;

	Status status = cstub->Connect(&context, request, &reply);
	if(!status.ok())
	{
		cout << "Could not connect to coordinator. Terminating" << endl;
		return;
	}
	string slaveInfo = reply.ipaddress() + ":" + std::to_string(reply.port());
	slaveStub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
				grpc::CreateChannel(
					slaveInfo, grpc::InsecureChannelCredentials())));
}

//Function where server connects with coordinator
void Coordinate(string coordAddress, string coordPort, string address, string port, int id) {
	string login_info = coordAddress + ":" + coordPort;
	cstub = std::unique_ptr<CoordService::Stub>(CoordService::NewStub(
				grpc::CreateChannel(
					login_info, grpc::InsecureChannelCredentials())));
	coordinator::Request request;
    request.set_type("server");
	request.set_id(id);
	if(master)
		request.add_arguments("master");
	else
		request.add_arguments("slave");
	request.add_arguments(address);
    request.add_arguments(port);
    coordinator::Reply reply;
    ClientContext context;

    Status status = cstub->Connect(&context, request, &reply);
	if(!status.ok())
	{
		cout << "Could not connect to coordinator. Terminating" << endl;
		return;
	}
}

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {

	string port = "3010";
	string coordAddress = "127.0.0.1";
	string coordPort = "1234";
	int id = 1;
	string masterOrNot;
	
	int opt = 0;
	while ((opt = getopt(argc, argv, "h:c:p:i:t:")) != -1){
		switch(opt) {
		case 'p':
			port = optarg;
			fileHeader = "cache/" + port;
			break;
		case 'h':
			coordAddress = optarg;
			break;
		case 'c':
			coordPort = optarg;
			break;
		case 'i':
			id = atoi(optarg);
			break;
		case 't':
			masterOrNot = optarg;
			if(masterOrNot == "master")
			{
				master = true;
			}
			else
			{
				master = false;
			}
			break;
		default:
		std::cerr << "Invalid Command Line Argument\n";
		}
	}

	//Sets up server cache files (location for data files)
	mkdir("cache", 0777);
	mkdir(fileHeader.c_str(), 0777);

	//How server determines its IP address
	string address = "";
	struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *addr;
	int count = 1;

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
			if(count == 0)
			{
				//Should send the ipv4 address of the server
				//If this is not working, simply setting address = "127.0.0.1" will allow this to work
				address = addr;
				break;
			}
			count--;
        }
    }
    freeifaddrs(ifap);

	Coordinate(coordAddress, coordPort, address, port, id);
	if(master)
		locateSlave(id);
	thread heartbeatThread(heartbeat, id);
	heartbeatThread.detach();

	thread synchronizer(synchronize, address, port);
	synchronizer.detach();

	RunServer(port);

	return 0;
}
