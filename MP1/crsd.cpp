#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

using std::cout, std::endl, std::string, std::thread, std::vector;

struct room {
    public:
        string name;
        int num_members;
        int port_no;
        room(string roomName, int port)
        {
            name = roomName;
            num_members = 0;
            port_no = port;
        }
};

vector<room> rooms;
int currentPort = 1500;

void process_request(int sockfd, char* buffer)
{
    Reply* serverReply = new Reply;
    if(buffer[0] == 'c')
    {
        string name = "";
        for(int i = 1; buffer[i] != '\0'; i++)
        {
            name += buffer[i];
        }
        for(int i = 0; i < rooms.size(); i++)
        {
            if(name == rooms.at(i).name)
            {
                serverReply->status = FAILURE_ALREADY_EXISTS;
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) != 0)
                    perror("Server send");
                return;
            }
        }

        room newRoom(name, currentPort);
        rooms.push_back(newRoom);
        cout << "Room created!" << endl;
        serverReply->status = SUCCESS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) != 0)
            perror("Server send");
        return;
    }
    else if(buffer[0] == 'd')
    {
        cout << "OKAY :(" << endl;
    }
    else if(buffer[0] == 'j')
    {
        cout << "OKAY :(" << endl;
    }
    else if(buffer[0] == 'l')
    {
        cout << "OKAY :(" << endl;
    }
    else
    {
        cout << "OKAY :(" << endl;
    }
}

void handle_client(int sockfd) {
    cout << "OKAY" << endl;
    int bufferCapacity = 256;
    char* buffer = new char[bufferCapacity];
	if (!buffer){
        cout << "Cannot allocate memory for server buffer" << endl;
        exit(1);
	}
	while (true){
		int nbytes = recv(sockfd, buffer, bufferCapacity, 0);
		if (nbytes < 0){
			cout << "Client-side terminated abnormally" << endl;
			break;
		}else if (nbytes == 0){
			// could not read anything in current iteration
			cout << "Could not read anything" << endl;
			break;
		}
		// MESSAGE_TYPE m = *(MESSAGE_TYPE *) buffer;
		// if (m == QUIT_MSG){
		// 	break;
		// 	// note that QUIT_MSG does not get a reply from the server
		// }
		process_request(sockfd, buffer);
	}
	delete [] buffer;
    close(sockfd);
}

void processing_loop(int sockfd)
{
    while(true)
    {
        cout << "Entered PL" << endl;
        struct sockaddr_storage otherAddr;
        socklen_t size = sizeof(otherAddr);
        int newSockFD;
        if((newSockFD = accept(sockfd, (struct sockaddr *) &otherAddr, &size)) == -1)
        {
            perror("Server accept");
        }
        thread newThread(handle_client, newSockFD);
        newThread.join();
    }
}

int main(int argc, char** argv)
{
    if(argc != 2 && argc != 3)
    {
        cout << "Incorrect Command Line Arguments!" << endl;
        exit(1);
    }

    string this_port = argv[1];
    cout << "Argv[1]: " << this_port << endl;
    currentPort = atoi(argv[1]);
    currentPort++;

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd;

    // !! don't forget your error checking for these calls !!

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if(getaddrinfo(NULL, this_port.c_str(), &hints, &res) == -1)
        perror("Getaddrinfo error: ");

    cout << "PASSED GEI" << endl;

    // make a socket, bind it, and listen on it:

    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        perror("socket error: ");
    
    cout << "PASSED socket" << endl;
    
    if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("bind error: ");
    }

    cout << "PASSED bind" << endl;
    
    if(listen(sockfd, 10) == -1)
    {
        close(sockfd);
        perror("listen error: ");
    }

    cout << "PASSED listen" << endl;

    // now accept an incoming connection:

    // addr_size = sizeof their_addr;
    // new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    // cout << "PASSED accept" << endl;

    // ready to communicate on socket descriptor new_fd!

    processing_loop(sockfd);

    cout << "Server terminated" << endl;

    // while(true)
    // {
    //     struct sockaddr_storage otherAddr;
    //     socklen_t size = sizeof(otherAddr);
    //     int newSockFD;
    //     if((newSockFD = accept(sockfd, (struct sockaddr *) &otherAddr, &size)) != 0)
    //     {
    //         perror("Server accept");
    //     }
    //     thread newThread(handle_client, newSockFD);
    //     newThread.join();
    // }
}