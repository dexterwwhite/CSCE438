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
vector<vector<string>> timelines;
vector<pair<string, ServerReaderWriter<Message, Message>*>> timelineFDs;
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

        cout << "Before loop" << endl;
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
        cout << "After ifs loop" << endl;

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
        cout << "OFS has closed" << endl;
      }
    }

    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    if(request->username() == request->arguments(0))
    {
      reply->add_all_users("failed-self");
      return Status::OK;
    }

    for(int i = 0; i < users.size(); i++)
    {
      cout << "User: " << users.at(i).first.at(0) << endl;
      for(int j = 0; j < users.at(i).first.size(); j++)
      {
        cout << "FOLLOWING: " << users.at(i).first.at(j) << endl;
      }
      for(int j = 0; j < users.at(i).second.size(); j++)
      {
        cout << "FOLLOWERS: " << users.at(i).second.at(j) << endl;
      }
    }

    bool exists = false;
    bool isFollowing = false;

    int index = 0;
    int followingPos = 0;
    int fIndex = 0;
    int followedPos = 0;

    {
      unique_lock<mutex> unfollowLock(mtx);
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
              isFollowing = true;
              followingPos = j;
              break;
            }
          }
        }

        //Finds whether the user-to-be-followed exists, updates fIndex if so
        if(request->arguments(0) == users.at(i).first.at(0))
        {
          exists = true;
          fIndex = i;

          for(int j = 0; j < users.at(i).second.size(); j++)
          {
            //If user already follows user-to-be-followed, FOLLOW command fails
            if(request->username() == users.at(i).second.at(j))
            {
              followedPos = j;
              break;
            }
          }
        }
      }

      if(!exists)
      {
        reply->add_all_users("failed-DNE");
      }
      else if(!isFollowing)
      {
        reply->add_all_users("failed-notfollow");
      }
      else
      {
        users.at(index).first.erase(users.at(index).first.begin() + followingPos);
        users.at(fIndex).second.erase(users.at(fIndex).second.begin() + followedPos);

        for(int i = 0; i < users.size(); i++)
        {
          cout << "User: " << users.at(i).first.at(0) << endl;
          for(int j = 0; j < users.at(i).first.size(); j++)
          {
            cout << "FOLLOWING: " << users.at(i).first.at(j) << endl;
          }
          for(int j = 0; j < users.at(i).second.size(); j++)
          {
            cout << "FOLLOWERS: " << users.at(i).second.at(j) << endl;
          }
        }

        ifs.open("database.txt");
        index = 0;
        int pos, fPos, removeIndex;
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
            string addition = db.at(i);
            removeIndex = addition.find(request->arguments(0));
            int offset = removeIndex + request->arguments(0).length() + 1;
            addition = addition.substr(0, removeIndex - 5) + addition.substr(offset, addition.length() - offset) + "\n";
            ofs << addition;
          }
          else if(i == fPos)
          {
            string addition = db.at(i);
            removeIndex = addition.find(request->username());
            int offset = removeIndex + request->username().length() + 1;
            addition = addition.substr(0, removeIndex - 5) + addition.substr(offset, addition.length() - offset) + "\n";
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
      bool found = false;
      for(int i = 0; i < users.size(); i++)
      {
        if(request->username() == users.at(i).first.at(0))
        {
          found = true;
          break;
        }
      }

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

        ofs.open("timeline.txt", std::ofstream::out | std::ofstream::app);
        ofs << "<*" + request->username() << "*>\n";
        ofs << "\n";
        ofs.close();

        vector<string> following;
        vector<string> followers;
        following.push_back(request->username());
        followers.push_back(request->username());
        pair<vector<string>, vector<string>> userFollows(following, followers);
        users.push_back(userFollows);

        vector<string> userTL;
        userTL.push_back(request->username());
        timelines.push_back(userTL);

        pair<string, ServerReaderWriter<Message, Message>*> userSRW;
        userSRW.first = request->username();
        timelineFDs.push_back(userSRW);
      }
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    
    Message msg;
    stream->Read(&msg);
    cout << "Timeline user: " << msg.username() << endl;
    cout << "Timeline msg: " << msg.msg() << endl;

    //Status status = stream->Finish();
    cout << "Okay" << endl;
    //if(status.ok())
    //{
      {
        cout << "Funny" << endl;
        unique_lock<mutex> tlLock(mtx);

        //pair<string, ServerReaderWriter<Message, Message>*> userSocket(msg.username(), stream);
        //timelineFDs.push_back(userSocket);

        cout << "Before loop" << endl;
        int index;
        for(int i = 0; i < timelines.size(); i++)
        {
          if(timelines.at(i).at(0) == msg.username())
          {
            timelineFDs.at(i).second = stream;
            index = i;
            break;
          }
        }

        cout << "timelines.at(index).size() == " << timelines.at(index).size() << endl;
        for(int i = timelines.at(index).size() - 1; i > 0; i--)
        {
          Message tlmsg;
          istringstream iss(timelines.at(index).at(i));
          string word;
          iss >> word;

          iss >> word;
          string header = word.substr(1, word.length() - 2);
          tlmsg.set_username(header);

          int start, end;
          iss >> word;
          start = iss.str().find(word);
          while(true)
          {
            iss >> word;
            if(word.find(")") != string::npos)
            {

              end = iss.str().find(word) + word.length();
              break;
            }
          }
          cout << "start: " << start << endl;
          cout << "end: " << end << endl;
          cout << "Str: " << iss.str() << endl;

          string body = "";
          for(int i = start; i < end; i++)
          {
            body += iss.str().at(i);
          }

          body += " >> ";
          while(!iss.eof())
          {
            iss >> word;
            if(word.length() == 0)
              break;
            body += word + " ";
            word.clear();
          }
          tlmsg.set_msg(body);
          stream->Write(tlmsg);
        }
        Message ender;
        string endermsg = "server--end**";
        ender.set_username(endermsg);
        stream->Write(ender);

        // Status status = stream->Finish();
        // if(!status.ok())
        // {
        //   return status;
        // }
      }

      cout << "outside loop" << endl;
      while(true)
      {
        Message loopmsg;
        if(stream->Read(&loopmsg))
        {
          cout << "loop user: " << loopmsg.username() << endl;
          cout << "loop message: " << loopmsg.msg() << endl;
          time_t realTime = google::protobuf::util::TimeUtil::TimestampToTimeT(loopmsg.timestamp());
          struct tm * timeinfo;
          timeinfo = localtime(&realTime);
          string time = asctime(timeinfo);
          time = time.substr(0, time.length() - 1);
          cout << "loop time: " << time << endl;

          string strMsg = "<> <";
          strMsg += loopmsg.username() + "> (" + time + ") ";
          strMsg += loopmsg.msg();

          Message newMsg;
          string firstPart = loopmsg.username();
          string second = "(" + time + ") >> ";
          second += loopmsg.msg();
          newMsg.set_username(firstPart);
          newMsg.set_msg(second);

          //Write to timeline.txt
          {
            unique_lock<mutex> tlWriteLock(mtx);

            int sendIndex;
            for(int i = 0; i < users.size(); i++)
            {
              if(users.at(i).first.at(0) == loopmsg.username())
              {
                sendIndex = i;
                break;
              }
            }

            cout << "Timelines before!" << endl;
            for(int i = 0; i < timelines.size(); i++)
            {
              for(int j = 0; j < timelines.at(i).size(); j++)
              {
                cout << timelines.at(i).at(j) << endl;
              }
              cout << endl;
            }

            for(int i = 0; i < users.at(sendIndex).second.size(); i++)
            {

              for(int j = 0; j < timelines.size(); j++)
              {
                if(timelines.at(j).at(0) == users.at(sendIndex).second.at(i))
                {
                  if(timelines.at(j).size() > 20)
                  {
                    timelines.at(j).erase(timelines.at(j).begin() + 1);
                  }
                  timelines.at(j).push_back(strMsg);
                }
              }
              
              if(i > 0)
              {
                for(int j = 0; j < timelineFDs.size(); j++)
                {
                  if(timelineFDs.at(j).first == users.at(sendIndex).second.at(i))
                  {
                    timelineFDs.at(j).second->Write(newMsg);
                  }
                }
              }
            }

            cout << "Timelines after!" << endl;
            for(int i = 0; i < timelines.size(); i++)
            {
              for(int j = 0; j < timelines.at(i).size(); j++)
              {
                cout << timelines.at(i).at(j) << endl;
              }
              cout << endl;
            }

            ofs.open("timeline.txt", std::ofstream::out | std::ofstream::trunc);
            for(int i = 0; i < timelines.size(); i++)
            {
              for(int j = 0; j < timelines.at(i).size(); j++)
              {
                if(j != 0)
                  ofs << timelines.at(i).at(j);
                else
                  ofs << "<*" << timelines.at(i).at(j) << "*>" << "\n";
              }
              ofs << "\n";
            }
            ofs.close();
          }

          //send to everyone
          //stream->Write(newMsg);
        }
      }



    // }
    // else
    // {
    //   return status;
    // }
    
    // Message m2;
    // stream->Read(&m2);
    // time_t tem = google::protobuf::util::TimeUtil::TimestampToTimeT(m2.timestamp());
    // struct tm * timeinfo;
    // timeinfo = localtime (&tem);
    // cout << "User: " << m2.username() << endl;
    // cout << "Message: " << m2.msg() << endl;
    // cout << "Time: " << asctime(timeinfo) << endl;
    cout << "Done" << endl;
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

  {
    unique_lock<mutex> mainLock(mtx);
    //Wipes any data from the database, if needed
    if(resetDB)
    {
      ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
      ofs << "USERS:";
      ofs.close();
      ofs.open("timeline.txt", std::ofstream::out | std::ofstream::trunc);
      ofs.close();
      cout << "Databases have been reset!" << endl;
    }
    cout << "Before database!" << endl;

    //Checks if a database file exists on start, makes a new one if needed
    ifs.open("database.txt");
    if(!ifs.is_open())
    {
      ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
      ofs << "USERS:";
      ofs.close();
      cout << "User database was not present, new database created!" << endl;
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
              if(name != "****" && name != "@****")
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
              if(name != "****" && name != "@****")
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

    cout << "Before timeline!" << endl;
    ifs.open("timeline.txt");
    if(!ifs.is_open())
    {
      ofs.open("timeline.txt", std::ofstream::out | std::ofstream::trunc);
      ofs.close();
      cout << "Timeline Database was not present, new database created!" << endl;
    }
    else
    {
      string line;

      while(!ifs.eof())
      {
        getline(ifs, line);
        if(line.length() >= 4)
        {
          if(line.substr(0, 2) == "<*" && line.substr(line.length() -2, 2) == "*>")
          {
            string name = line.substr(2, line.length() - 4);
            vector<string> userVec;
            userVec.push_back(name);
            timelines.push_back(userVec);

            pair<string, ServerReaderWriter<Message, Message>*> userSRW;
            userSRW.first = name;
            timelineFDs.push_back(userSRW);
          }
          else
          {
            timelines.back().push_back(line);
          }
        }

        line.clear();
      }
      ifs.close();
    }

  }

  cout << "After timeline!" << endl;

  RunServer(port);
  return 0;
}
