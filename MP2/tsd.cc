#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <sstream>
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
using std::istringstream;

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

    //Acquires mutex because accessing shared resource
    {
      unique_lock<mutex> listLock(mtx);

      //Searches global vector for users
      for(int i = 0; i < users.size(); i++)
      {
        //First user in each vector is the actual user
        if(request->username() == users.at(i).first.at(0))
        {
          for(int j = 0; j < users.at(i).second.size(); j++)
          {
            reply->add_following_users(users.at(i).second.at(j));
          }
        }
        reply->add_all_users(users.at(i).first.at(0));
      }
    }

    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------

    //Bool for whether the user that was requested to be follow actually exists
    bool exists = false;
    bool alreadyFollows = false;

    //Used to keep track of vector positions of user, user being followed
    int index = 0;
    int fIndex = 0;

    //Acquires mutex due to accessing shared resources
    {
      unique_lock<mutex> followLock(mtx);
      for(int i = 0; i < users.size(); i++)
      {
        if(request->username() == users.at(i).first.at(0))
        {
          //Sets index for easy updating of users vector later
          index = i;
          for(int j = 0; j < users.at(i).first.size(); j++)
          {
            //If user already follows user-to-be-followed, FOLLOW command fails
            if(request->arguments(0) == users.at(i).first.at(j))
            {
              reply->add_all_users("failed-follows");
              return Status::OK;
            }
          }
        }

        //Finds whether the user-to-be-followed exists, updates fIndex if so
        if(request->arguments(0) == users.at(i).first.at(0))
        {
          exists = true;
          fIndex = i;
        }
      }

      if(!exists)
      {
        reply->add_all_users("failed-DNE");
      }
      else
      {
        users.at(index).first.push_back(request->arguments(0));
        users.at(fIndex).second.push_back(request->username());

        ifs.open("database.txt");
        index = 0;
        int pos, fPos;
        string line;
        vector<string> db;

        while(!ifs.eof())
        {
          getline(ifs, line);
          db.push_back(line);

          if(line == request->username())
          {
            pos = index + 1;
          }
          else if(line == request->arguments(0))
          {
            fPos = index + 2;
          }

          index++;
          line.clear();
        }
        ifs.close();

        ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
        for(int i = 0; i < db.size(); i++)
        {
          if(i == pos)
          {
            string addition = db.at(i).substr(0, db.at(i).length() - 5);
            addition += "**** " + request->arguments(0) + " @****\n";
            ofs << addition;
          }
          else if(i == fPos)
          {
            string addition = db.at(i).substr(0, db.at(i).length() - 5);
            addition += "**** " + request->username() + " @****\n";
            ofs << addition;
          }
          else
          {
            if(i != db.size() - 1)
              ofs << db.at(i) << "\n";
            else
              ofs << db.at(i);
          }
        }

        ofs.close();
      }
    }

    
    
    if(alreadyFollows)
    {
      reply->add_all_users("failed-follows");
    }

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

  //Wipes any data from the database, if needed
  if(resetDB)
  {
    ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
    ofs << "USERS:";
    ofs.close();
    cout << "Database has been reset!" << endl;
  }

  //Checks if a database file exists on start, makes a new one if needed
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
    //Fills global vector with pre-existing data, to handle persistance of data
    string line;

    while(!ifs.eof())
    {
      getline(ifs, line);
      if(line.length() >= 15)
      {
        if((line.substr(0, 10) == "Following:") && (line.substr(line.length() - 5) == "@****"))
        {
          pair<vector<string>, vector<string>> newUser;
          istringstream iss(line);
          string name;
          vector<string> following;
          vector<string> followers;

          iss >> name;
          name.clear();
          while(!iss.eof())
          {
            iss >> name;
            if(name != "****" || name != "@****")
            {
              following.push_back(name);
            }
            name.clear();
          }

          newUser.first = following;
          newUser.second = followers;
          users.push_back(newUser);
        }
        else if((line.substr(0, 10) == "Followers:") && (line.substr(line.length() - 5) == "@****"))
        {
          istringstream iss(line);
          string name;

          iss >> name;
          name.clear();
          while(!iss.eof())
          {
            iss >> name;
            if(name != "****" || name != "@****")
            {
              users.back().second.push_back(name);
            }
            name.clear();
          }
        }
      }
    }

    ifs.close();
  }

  RunServer(port);
  return 0;
}
