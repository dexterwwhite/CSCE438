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
    string param = hostname + ":" + port;
    this->stub_ = (SNSService::NewStub(grpc::CreateChannel(param, grpc::InsecureChannelCredentials())));

    ClientContext cc;
    Reply rep;
    Request req;
    req.set_username(username);
    //req.set_arguments(1, "LOGIN");

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
	IReply ire;
    
    string cmd = "";
    int i;
    for(i = 0; i < input.length(); i++)
    {
        if(input.at(i) == ' ')
            break;
        cmd += input.at(i);
    }
    
    if(cmd == "FOLLOW")
    {
        string fUser = "";
        for(int i = 7; i < input.length(); i++)
        {
            fUser += input.at(i);
        }

        ClientContext sc;
        Reply rep;
        Request req;
        req.set_username(username);
        req.add_arguments(fUser);

        Status status = stub_->Follow(&sc, req, &rep);

        if(status.ok())
        {
            ire.grpc_status = status;
            if(rep.all_users_size() > 0)
            {
                if(rep.all_users(0) == "failed-follows")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
                else if(rep.all_users(0) == "failed-DNE")
                {
                    ire.comm_status = FAILURE_NOT_EXISTS;
                    return ire;
                }
            }
            else
            {
                ire.comm_status = SUCCESS;
                return ire;
            }
        }
        else
        {
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(cmd == "UNFOLLOW")
    {
        string fUser = "";
        for(int i = 9; i < input.length(); i++)
        {
            fUser += input.at(i);
        }

        ClientContext sc;
        Reply rep;
        Request req;
        req.set_username(username);
        req.add_arguments(fUser);

        Status status = stub_->UnFollow(&sc, req, &rep);

        if(status.ok())
        {
            ire.grpc_status = status;
            if(rep.all_users_size() > 0)
            {
                if(rep.all_users(0) == "failed-notfollow")
                {
                    ire.comm_status = FAILURE_INVALID_USERNAME;
                    return ire;
                }
                else if(rep.all_users(0) == "failed-DNE")
                {
                    ire.comm_status = FAILURE_NOT_EXISTS;
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
                ire.comm_status = SUCCESS;
                return ire;
            }
        }
        else
        {
            cout << status.error_code() << endl;
            cout << status.error_message() << endl;
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(input == "LIST")
    {
        ClientContext cc;
        Reply rep;
        Request req;
        req.set_username(username);
        //req.set_arguments(1, "LOGIN");

        Status status = stub_->List(&cc, req, &rep);
        if(status.ok())
        {
            vector<string> all;
            vector<string> following;
            ire.grpc_status = status;
            ire.comm_status = SUCCESS;
            for(int i = 0; i < rep.all_users_size(); i++)
            {
                all.push_back(rep.all_users(i));
            }

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
            ire.grpc_status = status;
            return ire;
        }
    }
    else if(input == "TIMELINE")
    {
        ire.grpc_status = Status::OK;
        ire.comm_status = SUCCESS;
        return ire;
    }
    else
    {
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
    
    //IReply ire;
    return ire;
}

void timelineThread(std::shared_ptr<ClientReaderWriter<Message, Message>> crw)
{
    while(true)
    {
        cout << "Am i blocking?" << endl;
        Message post;
        if(crw->Read(&post))
        {
            cout << post.username() << post.msg() << endl;
        }
    }
}

void Client::processTimeline()
{
    ClientContext cc;
    std::shared_ptr<ClientReaderWriter<Message, Message>> crw(stub_->Timeline(&cc));
    Message m1;
    m1.set_username(username);
    string mes1 = "conn-establish";
    m1.set_msg(mes1);
    crw->Write(m1);
    //crw->WritesDone();
    
    // Status status = crw->Finish();
    // cout << "MAde it here3" << endl;

    Message tlmsg;
    while(crw->Read(&tlmsg))
    {
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
    //status = crw->Finish();

    // //Status status = stub_->Timeline(&cc, &crw);
    // Message m1;
    // m1.set_username(username);
    // string messo = "Hi friend!";
    // m1.set_msg(messo);

    // Timestamp ts = google::protobuf::util::TimeUtil::GetCurrentTime();
    // //ts->GetCurrentTime();
    // //Timestamp ts = google::protobuf::util::time_util::TimeTToTimestamp(rawtime);
    // m1.set_allocated_timestamp(&ts);

    // crw->Write(m1);

    vector<thread> receiver;
    receiver.emplace_back(timelineThread, crw);

    //thread receiver(Client::timelineThread, crw);

    // int fd = 0;
    // struct pollfd fds[1];
    // int timeout, pollval;

    cout << "Made it to here" << endl;
    while(true)
    {
        cout << "Entered loop!" << endl;
        // fds[0].fd = fd;
        // fds[0].events = 0;
        // fds[0].events |= POLLIN;
        // timeout = 1000;

        // pollval = poll(fds, 1, timeout);

        // if(pollval == 0)
        // {
        //     // cout << "READ!" << endl;
        //     // Message post;
        //     // if(crw->Read(&post))
        //     // {
        //     //     cout << post.username() << post.msg() << endl;
        //     // }
        //     cout << "";
        // }
        // else
        // {
        Message loopmsg;
        string myMsg = getPostMessage();
        loopmsg.set_username(username);
        loopmsg.set_msg(myMsg);
        Timestamp ts = google::protobuf::util::TimeUtil::GetCurrentTime();
        loopmsg.set_allocated_timestamp(&ts);
        crw->Write(loopmsg);
        loopmsg.release_timestamp();
        if(loopmsg.has_timestamp())
        {
            loopmsg.clear_timestamp();
        }
        //crw->WritesDone();
        //}
    }
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
}
