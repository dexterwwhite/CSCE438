#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include "sns.grpc.pb.h"
#include "client.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::thread;

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    
    //Sets up client stub
    string param = hostname + ":" + port;
    this->stub_ = (SNSService::NewStub(grpc::CreateChannel(param, grpc::InsecureChannelCredentials())));

    //Client calls "Login" service
    ClientContext cc;
    Reply rep;
    Request req;
    req.set_username(username);
    Status status = stub_->Login(&cc, req, &rep);

    if(status.ok())
        return 1;
    else
        return -1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    //Variable set up
    IReply ire;
    string cmd = "";
    int i;

    //Parses input for the actual command (separate from its argument)
    for(i = 0; i < input.length(); i++)
    {
        if(input.at(i) == ' ')
            break;
        cmd += input.at(i);
    }
    
    if(cmd == "FOLLOW")
    {
        //Parses input for its argument (aka the user to be followed)
        string fUser = "";
        for(int i = 7; i < input.length(); i++)
        {
            fUser += input.at(i);
        }

        //Client stubs calls "Follow" service
        ClientContext sc;
        Reply rep;
        Request req;
        req.set_username(username);
        req.add_arguments(fUser);
        Status status = stub_->Follow(&sc, req, &rep);

        //Handling of the different results
        if(status.ok())
        {
            //I chose to have any error be present in the "all_users" data variable within Reply
            ire.grpc_status = status;
            if(rep.all_users_size() > 0)
            {
                //Due to the above reasoning, these are the two failure cases
                if(rep.all_users(0) == "failed-follows")
                {
                    ire.comm_status = FAILURE_ALREADY_EXISTS;
                    return ire;
                }
                else if(rep.all_users(0) == "failed-DNE")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
            }
            else
            {
                //Successful call to Follow
                ire.comm_status = SUCCESS;
                return ire;
            }
        }
        else
        {
            //gRPC status did not return successful, so this was a gRPC error and not a user error
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(cmd == "UNFOLLOW")
    {
        //Parses input for its argument (aka the user to be unfollowed)
        string fUser = "";
        for(int i = 9; i < input.length(); i++)
        {
            fUser += input.at(i);
        }

        //Client stub calls "Unfollow" service
        ClientContext sc;
        Reply rep;
        Request req;
        req.set_username(username);
        req.add_arguments(fUser);
        Status status = stub_->UnFollow(&sc, req, &rep);

        //Handles different situations based on server response
        if(status.ok())
        {
            //I chose to have any error be present in the "all_users" data variable within Reply
            ire.grpc_status = status;
            if(rep.all_users_size() > 0)
            {
                //Due to the above reasoning, these are the 3 failure cases of unfollow
                if(rep.all_users(0) == "failed-notfollow")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
                else if(rep.all_users(0) == "failed-DNE")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
                else if(rep.all_users(0) == "failed-self")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
            }
            else
            {
                //Successful call to Unfollow
                ire.comm_status = SUCCESS;
                return ire;
            }
        }
        else
        {
            //gRPC status did not return successful, so this was a gRPC error and not a user error
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(input == "LIST")
    {
        //Client stub calls "List" service
        ClientContext cc;
        Reply rep;
        Request req;
        req.set_username(username);
        Status status = stub_->List(&cc, req, &rep);

        //List can not fail other than by gRPC error, so there are no failure cases to handle
        if(status.ok())
        {
            ire.grpc_status = status;
            ire.comm_status = SUCCESS;

            //Populate IReply all users list
            vector<string> all;
            for(int i = 0; i < rep.all_users_size(); i++)
            {
                all.push_back(rep.all_users(i));
            }

            //Populate IReply following users list
            vector<string> following;
            for(int i = 0; i < rep.following_users_size(); i++)
            {
                following.push_back(rep.following_users(i));
            }

            ire.all_users = all;
            ire.following_users = following;
            return ire;
        }
        else
        {
            //gRPC status did not return successful, so this was a gRPC error and not a user error
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(input == "TIMELINE")
    {
        //Client does not actually call "Timeline" from here, it is called later in the processTimeline function
        ire.grpc_status = Status::OK;
        ire.comm_status = SUCCESS;
        return ire;
    }
    else
    {
        //If there is an invalid input sent by the client that is not caught by getCommand() function
        ire.grpc_status = Status::OK;
        ire.comm_status = FAILURE_INVALID;
        return ire;
    }
/**
* - FOLLOW/UNFOLLOW/TIMELINE command:
 * IReply ireply;
 * ireply.grpc_status = return value of a service method
 * ireply.comm_status = one of values in IStatus enum
 *
 * - LIST command:
 * IReply ireply;
 * ireply.grpc_status = return value of a service method
 * ireply.comm_status = one of values in IStatus enum
 * reply.users = list of all users who connected to the server at least onece
 * reply.following_users = list of users who current who current user are following;
 */


    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    return ire;
}

/**
* This function is meant to be used by a separate thread that's sole purpose is 
* to read any messages the server sends over. Due to the multithreaded nature of 
* the client, it does not have to worry about blocking while waiting for messages.
*
* @param crw the ClientReaderWriter that is used for reading messages from the server
*/
void timelineThread(std::shared_ptr<ClientReaderWriter<Message, Message>> crw)
{
    //Infinite loops and reads/displays any incoming messages from the server
    while(true)
    {
        Message post;
        while(crw->Read(&post))
        {
            cout << post.username() << post.msg() << endl;
        }
    }
}

void Client::processTimeline()
{
    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------

    //Sets up ClientReaderWriter with a call to "Timeline" service
    ClientContext cc;
    std::shared_ptr<ClientReaderWriter<Message, Message>> crw(stub_->Timeline(&cc));

    //Initial message that lets server know who is making the Timeline connection
    Message m1;
    m1.set_username(username);
    string mes1 = "conn-establish";
    m1.set_msg(mes1);
    crw->Write(m1);

    //Reads and displays any past timeline posts on the user's timeline
    Message tlmsg;
    while(crw->Read(&tlmsg))
    {
        //Server sends a message "server--end**" when it is done sending past timeline posts
        if(tlmsg.username() != "server--end**")
        {
            cout << tlmsg.username();
            cout << tlmsg.msg();
            cout << endl;
        }
        else
        {
            break;
        }
    }

    //Creates a new thread for above function, timelineThread
    vector<thread> receiver;
    receiver.emplace_back(timelineThread, crw);

    //Infinite loops and sends any user posts taken from standard input
    while(true)
    {
        //Reads in from standard input, sets up loopmsg with user post information, and sends to server
        Message loopmsg;
        string myMsg = getPostMessage();
        loopmsg.set_username(username);
        loopmsg.set_msg(myMsg);
        Timestamp ts = google::protobuf::util::TimeUtil::GetCurrentTime();
        loopmsg.set_allocated_timestamp(&ts);
        crw->Write(loopmsg);

        //Prevents memory leaks from the timestamp
        loopmsg.release_timestamp();
        if(loopmsg.has_timestamp())
        {
            loopmsg.clear_timestamp();
        }
    }
}
