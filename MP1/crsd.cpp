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

//Struct to hold chatroom information
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

//Global Vector, each element is a vector of socket FDs
//for a specific chatroom
vector<vector<int>> allSockFDs;

//Global vector of current open chatrooms
vector<room> rooms;

//Global vector that keeps track of port number to use
int currentPort = 1500;

//Global mutex to prevent thread race conditions
mutex mtx;

/*
 * Set up a new master socket for a chatroom
 * 
 * @return pair<int, int> containing the socket FD created
 *   the chatroom as the first int, and the port number used
 *   as the second int
 */
pair<int, int> new_master_socket()
{
    //Creates a port using the global port variable
    int sockfd;
    string this_port = to_string(currentPort);

    //Loops until a master socket is successfully set up
    while(true)
    {
        //Socket set up
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        //Gets address info for new master socket
        if(getaddrinfo(NULL, this_port.c_str(), &hints, &res) == -1)
        {
           perror("Getaddrinfo error: ");
        }

        //Sets up a file socket file descriptor for new master socket
        if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("socket error: ");

            //Increments global port variable if this current port is unavailable
            this_port = to_string(currentPort++);
        }

        //Binds socket to memory
        if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("bind error");

            //Increments global port variable if this current port is unavailable
            this_port = to_string(currentPort++);
        }
        else
        {
            break;
        }
    }
    
    //Sets up master socket to listen for incoming connections
    if(listen(sockfd, 10) == -1)
    {
        close(sockfd);
        perror("New Master Socket Listen Error");
    }

    //Sets up return value
    pair<int, int> values(sockfd, currentPort);
    currentPort++;
    return values;
}

/*
 * Infinite loop for slave socket to handle chat room one client
 * in the chatroom
 *
 * @parameter sockfd    slave socket FD for this chatroom's client
 * @parameter position  the index of this chatroom's vector of 
 *                      socket FDs within global allSockFDs vector
 */
void handle_room(int sockfd, int position) 
{
    //Buffer for server to receive messages
    int bufferCapacity = 256;
    char* buffer = new char[bufferCapacity];

    //Not enough memory to allocate buffer
	if (!buffer){
        cout << "Cannot allocate memory for server buffer" << endl;
        exit(1);
	}

    //Infinite loop for handling messages
	while (true){
		int nbytes = recv(sockfd, buffer, bufferCapacity, 0);
		if (nbytes < 0)
        {
			//cout << "Client-side terminated abnormally" << endl;
			break;
		}
        else if (nbytes == 0){
			//Could not read anything in current iteration
            //Typically takes place when a client disconnects from socket
			//cout << "Could not read anything" << endl;

            //Checks whether sockfd still exists in allSockFDs, if not
            //this chatroom has been closed and loop should terminate
            for(int i = 0; i < allSockFDs.at(position).size(); i++)
            {
                if(allSockFDs.at(position).at(i) == sockfd)
                {
                    allSockFDs.at(position).erase(allSockFDs.at(position).begin() + i);
                }
            }
            rooms.at(position).num_members -= 1;
			break;
		}
		
        //Sets response from client to a string
        string msg = buffer;
        {
            //Utilizes mutex so there are no race conditions on global vectors
            unique_lock<mutex> fdLock(mtx);

            //Checks to make sure sockfd still exists in allSockFDs to prevent
            //a vector out of bounds error
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

            //If sockfd could not be found, terminate loop
            if(!isThere)
                break;

            //Sends message from this client to every other client in the chatroom
            for(int i = 0; i < allSockFDs.at(position).size(); i++)
            {
                if(allSockFDs.at(position).at(i) != sockfd)
                {
                    if(send(allSockFDs.at(position).at(i), msg.c_str(), sizeof(msg), 0) == -1)
                        perror("Room send");
                }
            }
        }
	}
	delete [] buffer;
    close(sockfd);
}

/*
 * Infinite loop allowing for chatroom master socket to accept any incoming
 * connections
 *
 * @parameter sockfd    master socket FD, used to accept connections
 * @parameter position  the index of this chatroom's vector of 
 *                      socket FDs within global allSockFDs vector
 */
void room_loop(int sockfd, int position)
{
    //Infinite loop of master socket accepting client connections
    while(true)
    {
        //Sets up socket utilities for accepting a connection
        struct sockaddr_storage otherAddr;
        socklen_t size = sizeof(otherAddr);
        int newSockFD;

        //accepts incoming socket connection
        if((newSockFD = accept(sockfd, (struct sockaddr *) &otherAddr, &size)) == -1)
        {
            perror("Room master socket accept");
            break;
        }
        {
            //Mutex to prevent race conditions on global vector
            unique_lock<mutex> fdLock(mtx);
            bool found = false;

            //In case position has changed since this function was called, searches
            //for and updates position
            for(int i = 0; i < rooms.size(); i++)
            {
                if(rooms.at(i).sockfd == sockfd)
                {
                    found = true;
                    position = i;
                    break;
                }
            }

            //Terminates loop if this master socket is no longer open
            if(!found)
            {
                break;
            }
            
            //Adds newly accepted socket FD to global vector of socket FDs
            allSockFDs.at(position).push_back(newSockFD);
        }
        thread newThread(handle_room, newSockFD, position);
        newThread.detach();
    }
}

/*
 * Processes a message from a server and creates and sends back a 
 * response based on this message
 *
 * @parameter sockfd    slave socket FD of main server socket, connected
 *                      to client
 * @parameter buffer    message received from client
 */
void process_request(int sockfd, char* buffer)
{
    //Will be sent back to server
    Reply* serverReply = new Reply;

    //Processes create room request from client
    if(buffer[0] == 'c')
    {
        //Parses message for name of the room
        string name = "";
        for(int i = 1; buffer[i] != '\0'; i++)
        {
            name += buffer[i];
        }

        //Checks whether this room already exists
        for(int i = 0; i < rooms.size(); i++)
        {
            //If room exists, inform client and do nothing
            if(name == rooms.at(i).name)
            {
                serverReply->status = FAILURE_ALREADY_EXISTS;
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
                    perror("Server send");
                return;
            }
        }
        
        //Sets up new master socket for new chatroom
        pair<int, int> roomInfo = new_master_socket();

        //Makes new room with roomInfo data, also creates new vector
        //of socket FDs for the room
        room newRoom(name, roomInfo.second, roomInfo.first);
        vector<int> newRoomSockFDs;
        int position;

        {
            //Uses mutex to prevent race conditions on global vectors
            unique_lock<mutex> roomLock(mtx);
            rooms.push_back(newRoom);
            allSockFDs.push_back(newRoomSockFDs);
            position = allSockFDs.size() - 1;
        }
        
        serverReply->status = SUCCESS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");

        thread roomThread(room_loop, roomInfo.first, position);
        roomThread.detach();
        return;
    }
    //Processes delete room request from client
    else if(buffer[0] == 'd')
    {
        //Parses message for room name
        string name = "";
        for(int i = 1; buffer[i] != '\0'; i++)
        {
            name += buffer[i];
        }

        //Searches to ensure room exists
        for(int i = 0; i < rooms.size(); i++)
        {
            if(name == rooms.at(i).name)
            {
                {
                    //Mutex to prevent race conditions on global vectors
                    unique_lock<mutex> deleteLock(mtx);

                    //Sends warning message to all connected clients and closes
                    //their sockets
                    string deleteMSG = "Warning, chat room closed\n";
                    for(int j = 0; j < allSockFDs.at(i).size(); j++)
                    {
                        if(send(allSockFDs.at(i).at(j), deleteMSG.c_str(), sizeof(deleteMSG), 0) == -1)
                            perror("Server Delete Send");
                        close(allSockFDs.at(i).at(j));
                    }
                    
                    //Removes from global vectors and closes chatroom's master socket FD
                    allSockFDs.erase(allSockFDs.begin() + i);
                    close(rooms.at(i).sockfd);
                    rooms.erase(rooms.begin() + i);
                    
                }

                //Sends success message to client
                serverReply->status = SUCCESS;
                if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
                    perror("Server send");
                return;
            }
        }

        //Otherwise, room did not exist and server does nothing
        serverReply->status = FAILURE_NOT_EXISTS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;

    }
    //Processes join request from client
    else if(buffer[0] == 'j')
    {
        //Parses message for chatroom name
        string name = "";
        for(int i = 1; buffer[i] != '\0'; i++)
        {
            name += buffer[i];
        }

        //Checks to ensure chatroom exists already
        for(int i = 0; i < rooms.size(); i++)
        {
            //If room exists, sends client information about chatroom
            //so client can connect to that socket
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

        //Otherwise, room does not exist and server does nothing
        serverReply->status = FAILURE_NOT_EXISTS;
        cout << "Room does not exist!" << endl;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;
    }
    //Processes list request from client
    else if(buffer[0] == 'l')
    {
        //Adds any existing rooms within global vector to response
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

        //Sends response to client
        serverReply->list_room[index] = '\0';
        serverReply->status = SUCCESS;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;
    }
    //Client did not properly prevent invalid command from going through
    else
    {
        serverReply->status = FAILURE_INVALID;
        if(send(sockfd, (char*)serverReply, sizeof(Reply), 0) == -1)
            perror("Server send");
        return;
    }
}

/*
 * Infinite loop to process messages from connected client
 *
 * @parameter sockfd    slave socket FD of server master socket,
 *                      currently connected to client
 */
void handle_client(int sockfd) {
    //Sets up buffer for receiving from client
    int bufferCapacity = 256;
    char* buffer = new char[bufferCapacity];
    //Not enough memory to allocate for buffer
	if (!buffer){
        cout << "Cannot allocate memory for server buffer" << endl;
        exit(1);
	}

    //Infnite loop to handle messages from client
	while (true){
		int nbytes = recv(sockfd, buffer, bufferCapacity, 0);
		if (nbytes < 0)
        {
			//cout << "Client-side terminated abnormally" << endl;
			break;
		}
        else if (nbytes == 0){
			//Could not read anything in current iteration
            //Typically means client disconnected
			//cout << "Could not read anything" << endl;
			break;
		}
		
        //If recv was successful, processes request from client
		process_request(sockfd, buffer);
	}
    //Issue with receive -> close socket
	delete [] buffer;
    close(sockfd);
}

/*
 * Server master socket infinitely loops, accepting any incoming
 * connections from client
 *
 * @parameter sockfd    master socket FD of the server, used to accept
 *                      any incoming connections from clients
 */
void processing_loop(int sockfd)
{
    //Infinitely loops accepting connections
    while(true)
    {
        //Sets up socket utilities for accepting a connection
        struct sockaddr_storage otherAddr;
        socklen_t size = sizeof(otherAddr);
        int newSockFD;

        //Accepts client connection and creates a new thread to handle slave socket
        if((newSockFD = accept(sockfd, (struct sockaddr *) &otherAddr, &size)) == -1)
        {
            perror("Server accept");
        }
        thread newThread(handle_client, newSockFD);
        newThread.detach();
    }
}

/*
 * Main function, sets up initial server master socket
 *
 * @parameter argc    Number of command line arguments
 * @parameter argv    Array of command line arguments
 * 
 * @return whether or not server terminated properly
 */
int main(int argc, char** argv)
{
    //Check to make sure proper number of command line arguments
    if(argc != 2 && argc != 3)
    {
        cout << "Incorrect Command Line Arguments!" << endl;
        exit(1);
    }
    //I was unsure if the & sign on the instructions pdf required
    //us to accept an & sign
    if(argc == 3)
    {
        string andSign = argv[2];
        if(andSign != "&")
        {
            cout << "Incorrect Command Line Arguments!" << endl;
            exit(1);
        }
    }

    //Collects port number from command line and
    //sets global port variable to that number + 1
    string this_port = argv[1];
    currentPort = atoi(argv[1]);
    currentPort++;

    //Sets up socket utilities
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd;

    //Sets up hints addrinfo struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //Gets address info of given port
    if(getaddrinfo(NULL, this_port.c_str(), &hints, &res) == -1)
        perror("Getaddrinfo");

    //Create socket file descriptor for the server master socket
    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        perror("Socket Error");
    
    //Binds socket to memory
    if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("Bind Error");
        cout << "Socket could not be bound, likely an issue with the port number!" << endl;
        cout << "Please try again with a new port number! Server will now shut down." << endl;
        exit(1);
    }
    
    //Sets up socket to listen for incoming connections
    if(listen(sockfd, 10) == -1)
    {
        close(sockfd);
        perror("Listen Error");
        cout << "Issue with socket listen, shutting down server!" << endl;
        exit(1);
    }

    //Socket is properly set up to accept incoming connections
    //Begins an infinite loop of accepting these connections
    processing_loop(sockfd);

    //Server stopped accepting connections and terminated
    cout << "Server terminated" << endl;
}