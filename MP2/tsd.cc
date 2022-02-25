#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

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
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::pair;
using std::mutex;
using std::unique_lock;

mutex mtx;
vector<pair<vector<string>, vector<string>>> users;
std::ifstream ifs;
std::ofstream ofs;

class SNSServiceImpl final : public SNSService::Service {

  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    //cout << "User: " << request->username << endl;
    //cout << "MSG: " << request->arguments << endl;
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    cout << "User: " << request->username() << endl;

    //File database setup
    {
      unique_lock<mutex> loginLock(mtx);
      ifs.open("database.txt");
      bool found = false;
      //ofs.open("database.txt");
      string line;

      int count = 1;
      while(!ifs.eof())
      {
        getline(ifs, line);
        cout << "Count: " << count++ << endl;
        cout << "Line: " << line << endl;

        if(line == request->username())
        {
          found = true;
          break;
        }

        line.clear();
      }
      ifs.close();

      if(!found)
      {
        ofs.open("database.txt", std::ofstream::out | std::ofstream::app);
        ofs << "\n\n";
        ofs << request->username() << "\n";
        ofs << "Following: " << request->username() << " @****\n";
        ofs << "Followers: " << request->username() << " @****\n";
        ofs << "\n";
        ofs << "@**@**";
        ofs.close();

        vector<string> following;
        vector<string> followers;
        following.push_back(request->username());
        followers.push_back(request->username());
        pair<vector<string>, vector<string>> userFollows(following, followers);
        users.push_back(userFollows);
      }
      //ofs.close();
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  
  //READ FROM TXT FILE TO SET UP VEC OF USERS
  //vec is vector<Pair<Vector<String>, Vector<String>>
  //first vector in pair is following
  //second vector is who user is followed by
  //VEC is format <user>, following1, following2
  users.size();

  //Setting up server "database" file IOstreams
  //ifs.open("database.txt");
  //ofs.open("database.txt");

  string server_addr = "0.0.0.0:" + port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server is listening on " << port_no << endl;
  server->Wait();
}

int main(int argc, char** argv) {
  string check;
  std::string port = "3010";
  bool resetDB = false;
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:r:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      case 'r':
          check = optarg;
          if(check == "RESET")
            resetDB = true;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }

  if(resetDB)
  {
    ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
    ofs << "USERS:";
    ofs.close();
    cout << "Database has been reset!" << endl;
  }

  ifs.open("database.txt");
  if(!ifs.is_open())
  {
    ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
    ofs << "USERS:";
    ofs.close();
    cout << "Database was not present, new database created!" << endl;
  }
  else
  {
    ifs.close();
  }

  RunServer(port);
  return 0;
}
