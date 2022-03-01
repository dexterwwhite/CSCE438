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

/**
* Global Variables used by server
* Lines 47-74
*/
mutex mtx;

/**
* "Users" vector keeps track of user information
* In each vector within users, the user that is being represented is the first element
*
* Pair's first: vector of usernames of users who the current user is following
* Pair's second: vector of usernames of users who are following the current user
*/
vector<pair<vector<string>, vector<string>>> users;

/**
* "Timelines" vector is a vector of vectors, where each of its elements is a list of a given user's timeline posts
* First element in each vector element is the username of the user who's timeline populates the vector
*/
vector<vector<string>> timelines;

/**
* "TimelineFDs" vector is how the server sends messages to all users that are following a given user
*
* Pair's first: username of a given user, and uses the associated ServerReaderWriter in the pair
* Pair's second: the ServerReaderWriter that is used by the given user, how messages are sent to this user
*/
vector<pair<string, ServerReaderWriter<Message, Message>*>> timelineFDs;

//Fstream objects used for file IO
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
        //Proceeds to populate reply's data variables
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

      //If user-to-be-followed does not exist, updates failure argument in reply
      if(!exists)
      {
        reply->add_all_users("failed-DNE");
      }
      else
      {
        //User is found, so global "users" vector is updated accordingly
        users.at(index).first.push_back(request->arguments(0));
        users.at(fIndex).second.push_back(request->username());

        //Reads in current database file information and populates a vector with each line
        ifs.open("database.txt");
        index = 0;
        int pos, fPos;
        string line;
        vector<string> db;
        while(!ifs.eof())
        {
          //Adds each line from the file to the vector
          getline(ifs, line);
          db.push_back(line);

          //Pos is the iteration where the user's list of followed users must be updated
          //fPos is the iteration where the user-to-be-followed's list of following users must be updated
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

        //Clears previous database file and opens it up for writing
        ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
        for(int i = 0; i < db.size(); i++)
        {
          //Updates the "Following" line of the user who followed the new user
          if(i == pos)
          {
            //Handles database text file structure
            string addition = db.at(i).substr(0, db.at(i).length() - 5);
            addition += "**** " + request->arguments(0) + " @****\n";
            ofs << addition;
          }
          //Updates the "Followers" line of the user who has just been followed
          else if(i == fPos)
          {
            //Handles database text file structure
            string addition = db.at(i).substr(0, db.at(i).length() - 5);
            addition += "**** " + request->username() + " @****\n";
            ofs << addition;
          }
          else
          {
            //Otherwise just adds the line to the file like normal
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

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------

    //If user is attempting to unfollow self, no works needs to be done
    if(request->username() == request->arguments(0))
    {
      reply->add_all_users("failed-self");
      return Status::OK;
    }

    //Variables used for keeping track of user existence and positions
    bool exists = false;
    bool isFollowing = false;
    int index = 0;
    int followingPos = 0;
    int fIndex = 0;
    int followedPos = 0;

    {
      //Acquires mutex due to accessing of shared resources
      unique_lock<mutex> unfollowLock(mtx);
      for(int i = 0; i < users.size(); i++)
      {
        //Finds user within users vector that is attempting to unfollow other user
        if(request->username() == users.at(i).first.at(0))
        {
          //Sets index for easy updating of users vector later
          index = i;
          for(int j = 0; j < users.at(i).first.size(); j++)
          {
            //Checks whether user actually follows user-to-be-unfollowed, updates variables accordingly
            if(request->arguments(0) == users.at(i).first.at(j))
            {
              isFollowing = true;
              followingPos = j;
              break;
            }
          }
        }

        //Finds whether the user-to-be-unfollowed exists, updates exists and fIndex variables if so
        if(request->arguments(0) == users.at(i).first.at(0))
        {
          exists = true;
          fIndex = i;

          for(int j = 0; j < users.at(i).second.size(); j++)
          {
            //Updates followedPos variable with index of username of user unfollowing within following user's vector of user-to-be-unfollowed
            //The above sentence is a poorly formed sentence
            if(request->username() == users.at(i).second.at(j))
            {
              followedPos = j;
              break;
            }
          }
        }
      }

      //Handles failure cases of unfollow
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
        //Unfollow is successful, updates "users" vector accordingly
        users.at(index).first.erase(users.at(index).first.begin() + followingPos);
        users.at(fIndex).second.erase(users.at(fIndex).second.begin() + followedPos);

        //Reads in current database file information and populates a vector with each line
        ifs.open("database.txt");
        index = 0;
        int pos, fPos, removeIndex;
        string line;
        vector<string> db;
        while(!ifs.eof())
        {
          //Adds each line of file to the vector
          getline(ifs, line);
          db.push_back(line);

          //Sets pos equal to the line number of the user that is unfollowing the other user
          if(line == request->username())
          {
            pos = index + 1;
          }
          //fPos is the line number of the user who is being unfollowed
          else if(line == request->arguments(0))
          {
            fPos = index + 2;
          }

          index++;
          line.clear();
        }
        ifs.close();

        //Erases database files data and repopulates it with contents from vector and with new updates
        ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
        for(int i = 0; i < db.size(); i++)
        {
          //Removes the unfollowed user from the unfollowing user's following list
          if(i == pos)
          {
            string addition = db.at(i);
            removeIndex = addition.find(request->arguments(0));
            int offset = removeIndex + request->arguments(0).length() + 1;
            addition = addition.substr(0, removeIndex - 5) + addition.substr(offset, addition.length() - offset) + "\n";
            ofs << addition;
          }
          //Removes the unfollowing user from the unfollowed user's follower list
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
            //Otherwise, adds each line back to the file like normal
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
    {
      //Acquires mutex due to accessing of shared resources
      unique_lock<mutex> loginLock(mtx);

      //Checks whether the user logging in is already present in the database
      bool found = false;
      for(int i = 0; i < users.size(); i++)
      {
        if(request->username() == users.at(i).first.at(0))
        {
          found = true;
          break;
        }
      }

      //If user is not found within the database, new user information must be created
      if(!found)
      {
        //Adds a section in the user database file for the new user and their followed/following users
        ofs.open("database.txt", std::ofstream::out | std::ofstream::app);
        ofs << "\n\n";
        ofs << request->username() << "\n";
        ofs << "Following: " << request->username() << " @****\n";
        ofs << "Followers: " << request->username() << " @****\n";
        ofs << "\n";
        ofs << "@**@**";
        ofs.close();

        //Adds a section in the timeline database file for the new user and their timeline
        ofs.open("timeline.txt", std::ofstream::out | std::ofstream::app);
        ofs << "<*" + request->username() << "*>\n";
        ofs << "\n";
        ofs.close();

        //Adds new vectors to represent a user's followers and followed users and adds to the "users" global vector
        vector<string> following;
        vector<string> followers;
        following.push_back(request->username());
        followers.push_back(request->username());
        pair<vector<string>, vector<string>> userFollows(following, followers);
        users.push_back(userFollows);

        //Adds a new vector to represent a user's timeline to the "timelines" global vector
        vector<string> userTL;
        userTL.push_back(request->username());
        timelines.push_back(userTL);

        //Adds a new pair to the "timelineFDs" vector, in case the user ever goes into timeline mode
        pair<string, ServerReaderWriter<Message, Message>*> userSRW;
        userSRW.first = request->username();
        userSRW.second = nullptr;
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
    
    //Reads the clients initial connection message
    Message msg;
    stream->Read(&msg);
  
    {
      //Acquires mutex since shared resources will be used
      unique_lock<mutex> tlLock(mtx);

      //Finds the user's username within timelineFDs global vector, and sets ServerReaderWriter pointer equal to stream
      //This will allow other client's messages to be sent to this current user
      int index;
      for(int i = 0; i < timelines.size(); i++)
      {
        //Additionally, finds user's index within timelines for ease of use in next loop
        if(timelines.at(i).at(0) == msg.username())
        {
          timelineFDs.at(i).second = stream;
          index = i;
          break;
        }
      }

      //Sends any of the user's past timeline posts to the user upon login
      for(int i = timelines.at(index).size() - 1; i > 0; i--)
      {
        Message tlmsg;
        istringstream iss(timelines.at(index).at(i));
        string word;

        //The first word from the timeline line is skipped
        iss >> word;

        //Strips the "<" and ">" from the username of the timeline post
        iss >> word;
        string header = word.substr(1, word.length() - 2);
        tlmsg.set_username(header);

        //Finds indexes of each parenthesis from the date which will be used to parse the date from the line
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

        //Parses the date from the line, which is enclosed in format (date)
        string body = "";
        for(int i = start; i < end; i++)
        {
          body += iss.str().at(i);
        }

        //Parses the remaining words from the line (which is actually just the post's message)
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

      //This message is sent to notify the client that the stream of past timeline posts has ended
      Message ender;
      string endermsg = "server--end**";
      ender.set_username(endermsg);
      stream->Write(ender);
    }

    //Continuously loops and reads in messages from the client and sends them to all users that are following the client
    while(true)
    {
      Message loopmsg;
      if(stream->Read(&loopmsg))
      {
        //Parses timestamp from message and converts it to a string version of the date
        time_t realTime = google::protobuf::util::TimeUtil::TimestampToTimeT(loopmsg.timestamp());
        struct tm * timeinfo;
        timeinfo = localtime(&realTime);
        string time = asctime(timeinfo);
        time = time.substr(0, time.length() - 1);

        //Formats message contents to timeline database format
        string strMsg = "<> <";
        strMsg += loopmsg.username() + "> (" + time + ") ";
        strMsg += loopmsg.msg();

        //Formats message that will be sent to all the user's followers
        Message newMsg;
        string firstPart = loopmsg.username();
        string second = "(" + time + ") >> ";
        second += loopmsg.msg();
        newMsg.set_username(firstPart);
        newMsg.set_msg(second);

        {
          //Acquires mutex due to accessing shared resources
          unique_lock<mutex> tlWriteLock(mtx);

          //Finds the index that correlates to the sending user's index within users global vector
          int sendIndex;
          for(int i = 0; i < users.size(); i++)
          {
            if(users.at(i).first.at(0) == loopmsg.username())
            {
              sendIndex = i;
              break;
            }
          }

          //This loop handles updates to timelines vector and sends out messages to client's followers
          for(int i = 0; i < users.at(sendIndex).second.size(); i++)
          {

            //Updates timelines global vector for all following users
            for(int j = 0; j < timelines.size(); j++)
            {
              if(timelines.at(j).at(0) == users.at(sendIndex).second.at(i))
              {
                //If user already has 20 posts on their timeline, removes the oldest post
                if(timelines.at(j).size() > 20)
                {
                  timelines.at(j).erase(timelines.at(j).begin() + 1);
                }
                timelines.at(j).push_back(strMsg);
              }
            }
            
            //Sends the message to every following user besides the sender
            if(i > 0)
            {
              for(int j = 0; j < timelineFDs.size(); j++)
              {
                if(timelineFDs.at(j).first == users.at(sendIndex).second.at(i))
                {
                  if(timelineFDs.at(j).second != nullptr)
                    timelineFDs.at(j).second->Write(newMsg);
                }
              }
            }
          }

          //Erases timeline database's contents and writes the complete contents of the timelines vector to it
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
      }
    }
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

  //Sets up gRPC server service
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

  //Added the option of resetting the database, mainly for ease of testing purposes
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
    //Acquires mutex due to accessing of shared resources
    unique_lock<mutex> mainLock(mtx);

    //Wipes any data from the user and timeline database text files, if needed
    if(resetDB)
    {
      ofs.open("database.txt", std::ofstream::out | std::ofstream::trunc);
      ofs << "USERS:";
      ofs.close();
      ofs.open("timeline.txt", std::ofstream::out | std::ofstream::trunc);
      ofs.close();
      cout << "Databases have been reset!" << endl;
    }

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

        //This length check is to mainly avoid unnecessary bounds errors
        if(line.length() >= 15)
        {
          //If a line is a followed users line, adds it to the global users vector
          if((line.substr(0, 10) == "Following:") && (line.substr(line.length() - 5) == "@****"))
          {
            pair<vector<string>, vector<string>> newUser;
            istringstream iss(line);
            string name;
            vector<string> following;
            vector<string> followers;

            //Adds important user information to vector, discards others
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

            //Sets up vectors for the users vector
            newUser.first = following;
            newUser.second = followers;
            users.push_back(newUser);
          }

          //If a line is a following users line, adds it to the global users vector
          else if((line.substr(0, 10) == "Followers:") && (line.substr(line.length() - 5) == "@****"))
          {
            istringstream iss(line);
            string name;

            ////Adds important user information to vector, discards others
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

    //Checks whether a timeline database file already exists
    ifs.open("timeline.txt");
    if(!ifs.is_open())
    {
      //If not, creates a new one
      ofs.open("timeline.txt", std::ofstream::out | std::ofstream::trunc);
      ofs.close();
      cout << "Timeline Database was not present, new database created!" << endl;
    }
    else
    {
      //If a file does exist, parses it for timeline information
      string line;
      while(!ifs.eof())
      {
        getline(ifs, line);

        //Another if statement to prevent bounds errors
        //Additionally, ALL lines present within the database will be longer than 4 characters, so skips any unnecessary lines
        if(line.length() >= 4)
        {
          //Checks if the line contains the username in format "<*username*>"
          if(line.substr(0, 2) == "<*" && line.substr(line.length() -2, 2) == "*>")
          {
            //Adds the username as the first element within a vector, and then adds this vector to the timelines vector
            string name = line.substr(2, line.length() - 4);
            vector<string> userVec;
            userVec.push_back(name);
            timelines.push_back(userVec);

            //Additionally, sets up the timelineFDs vector for the user
            pair<string, ServerReaderWriter<Message, Message>*> userSRW;
            userSRW.first = name;
            userSRW.second = nullptr;
            timelineFDs.push_back(userSRW);
          }
          else
          {
            //If not a username line, adds this line to the timelines vector for the latest user
            timelines.back().push_back(line);
          }
        }

        line.clear();
      }
      ifs.close();
    }

  }

  RunServer(port);
  return 0;
}
