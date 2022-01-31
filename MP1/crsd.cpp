#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

using std::cout, std::endl, std::string, std::thread, std::vector, std::pair, std::to_string, std::unique_lock, std::mutex;

/**
    NOTE:
    Run CRC With ./crc 127.0.0.1 1500
    Run crsd with ./crsd 1500

*/

struct room {
    public:
        string name;
        int num_members;
        int port_no;
        int sockfd;
        room(string roomName, int port, int fd)
        {
            name = roomName;
            num_members = 0;
            port_no = port;
            sockfd = fd;
        }
};

vector<vector<int>> allSockFDs;
vector<room> rooms;
int currentPort = 1500;
mutex mtx;

//returns pair<sockfd, port number> of new socket
pair<int, int> new_master_socket()
{
    int sockfd;
    string this_port = to_string(currentPort);

    while(true)
    {
        cout << "Current port: " << currentPort << endl;
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
        if(getaddrinfo(NULL, this_port.c_str(), &hints, &res) == -1)
        {
            perror("Getaddrinfo error: ");
        }

        // make a socket, bind it, and listen on it:
        if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("socket error: ");
            this_port = to_string(currentPort++);
        }
    
        if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("bind error: ");
            this_port = to_string(currentPort++);
        }
        else
        {
            break;
        }
    }
    
    if(listen(sockfd, 10) == -1)
    {
        close(sockfd);
        perror("listen error: ");
    }

    pair<int, int> values(sockfd, currentPort);
    currentPort++;
    return values;
}

void handle_room(int sockfd, int position) 
{
    cout << "Handling Room..." << endl;
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
		cout << "Message: " << buffer << endl;
        string msg = buffer;
        //msg += "\n";
        {
            unique_lock<mutex> fdLock(mtx);
            cout << "handle room before vec" << endl;
            // if(position >= allSockFDs.size())
            //     break;
            bool isThere = false;
            for(int i = 0; i < allSockFDs.size(); i++)
            {
                for(int j = 0; j < allSockFDs.at(i).size(); j++)
                {
                    if(allSockFDs.at(i).at(j) == sockfd)
                    {
                        isThere = true;
                        break;
                    }
                }
            }

            if(!isThere)
                break;
            for(int i = 0; i < allSockFDs.at(position).size(); i++)
            {
                if(allSockFDs.at(position).at(i) != sockfd)
                {
                    if(send(allSockFDs.at(position).at(i), msg.c_str(), sizeof(msg), 0) == -1)
                        perror("Room send");
                }
            }
            cout << "handle room after vec" << endl;
        }
        cout << "Made it out" << endl;
        
	}
	delete [] buffer;
    close(sockfd);
}

void room_loop(int sockfd, int position)
{
    //vector<int> sockFDVec;
    while(true)
    {
        cout << "Entered Room Loop" << endl;
        struct sockaddr_storage otherAddr;
        socklen_t size = sizeof(otherAddr);
        int newSockFD;
        if((newSockFD = accept(sockfd, (struct sockaddr *) &otherAddr, &size)) == -1)
        {
            perror("Server accept");
            break;
        }
        {
            unique_lock<mutex> fdLock(mtx);
            //sockFDVec.push_back(newSockFD);
            cout << "room loop before vec" << endl;
            allSockFDs.at(position).push_back(newSockFD);
            cout << "room loop after vec" << endl;
        }
        thread newThread(handle_room, newSockFD, position);
        newThread.detach();
    }
}

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
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
                    perror("Server send");
                return;
            }
        }
        
        pair<int, int> roomInfo = new_master_socket();

        room newRoom(name, roomInfo.second, roomInfo.first);
        vector<int> newRoomSockFDs;
        int position;
        {
            unique_lock<mutex> roomLock(mtx);
            rooms.push_back(newRoom);
            allSockFDs.push_back(newRoomSockFDs);
            position = allSockFDs.size() - 1;
        }
        cout << "Room created!" << endl;
        serverReply->status = SUCCESS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");

        
        thread roomThread(room_loop, roomInfo.first, position);
        roomThread.detach();
        return;
    }
    else if(buffer[0] == 'd')
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
                {
                    unique_lock<mutex> deleteLock(mtx);
                    string deleteMSG = "Warning, chat room closed\n";
                    for(int j = 0; j < allSockFDs.at(i).size(); j++)
                    {
                        cout << "Loop iteration" << endl;
                        if(send(allSockFDs.at(i).at(j), deleteMSG.c_str(), sizeof(deleteMSG), 0) == -1)
                            perror("Server Delete Send");
                        close(allSockFDs.at(i).at(j));
                    }
                    cout << "Before erase 1" << endl;
                    allSockFDs.erase(allSockFDs.begin() + i);
                    cout << "Before close" << endl;
                    close(rooms.at(i).sockfd);
                    cout << "Before erase 2" << endl;
                    rooms.erase(rooms.begin() + i);
                    cout << "MUTEX end" << endl;
                }
                serverReply->status = SUCCESS;
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
                    perror("Server send");
                return;
            }
        }

        serverReply->status = FAILURE_NOT_EXISTS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;

    }
    else if(buffer[0] == 'j')
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
                serverReply->status = SUCCESS;
                serverReply->num_member = rooms.at(i).num_members;
                serverReply->port = rooms.at(i).port_no;
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
                    perror("Server send");
                rooms.at(i).num_members += 1;
                return;
            }
        }

        serverReply->status = FAILURE_NOT_EXISTS;
        cout << "Room does not exist!" << endl;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;
    }
    else if(buffer[0] == 'l')
    {
        int index = 0;
        for(int i = 0; i < rooms.size(); i++)
        {
            string name = rooms.at(i).name;
            for(int j = 0; j < name.length(); j++)
            {
                serverReply->list_room[index++] = name.at(j);
            }
            if(i != rooms.size() - 1)
            {
                serverReply->list_room[index++] = ',';
            }
        }
        serverReply->list_room[index] = '\0';
        serverReply->status = SUCCESS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;
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
        newThread.detach();
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