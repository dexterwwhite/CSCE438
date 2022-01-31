#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "interface.h"


/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
//int connect_to(const char *host, const int port);
int connect_to(const char *host, const char* port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}
    display_title();
    
	while (1) {
		int sockfd = connect_to(argv[1], argv[2]);
    
		char command[MAX_DATA];
        get_command(command, MAX_DATA);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		//Checks whether client successfully joined a chatroom
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.status == SUCCESS) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);

			//If chatroom is closed while client is inside, displays title once again
			display_title();
		}
	
		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket file descriptor
 */
int connect_to(const char *host, const char* port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------
	struct addrinfo hints, *res;
	int sockfd;

	//Set up hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	//Get address info of the server, update hints and res
	if(getaddrinfo(host, port, &hints, &res) == -1)
		perror("GAI");

	//Create socket using res
	if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		perror("socket");

	//Connect to server with above socket file descriptor and res
	if(connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
		perror("connect");
	
	//Return socket file descriptor
	return sockfd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	struct Reply reply;
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------
	char name[100];
	while(1)
	{
		//Lines 133-174 check for and handle user command CREATE
		char* word = "CREATE";
		int i;

		//Checks character by character if command == 'CREATE'
		for(i = 0; command[i] != '\0' || i < 6; i++)
		{
			if(word[i] != command[i])
				break;
		}

		//If i != 6, we know command was not 'CREATE'
		if(i == 6)
		{
			//Check if command is valid format: CREATE <name>
			if(command[i] != '\0')
			{
				//Skip space character in command
				i++;
			}
			else
			{
				reply.status = FAILURE_INVALID;
				return reply;
			}

			//Format message to server in form: 'c<name>'
			name[0] = 'c';
			int j = 1;
			while(command[i] != '\0')
			{
				if(command[i] == ' ')
				{
					reply.status = FAILURE_INVALID;
					return reply;
				}
				name[j] = command[i];
				j++;
				i++;
			}
			name[j] = '\0';
			break;
		}

		//Lines 177-218 check for and handle user command DELETE
		i = 0;
		word = "DELETE";

		//Checks character by character if command == 'DELETE'
		for(i = 0; command[i] != '\0' || i < 6; i++)
		{
			if(word[i] != command[i])
				break;
		}

		//If i != 6, we know command was not 'DELETE'
		if(i == 6)
		{
			//Check if command is valid format: DELETE <name>
			if(command[i] != '\0')
			{
				//Skip the space character in command
				i++;
			}
			else
			{
				reply.status = FAILURE_INVALID;
				return reply;
			}

			//Format message to server in form: 'd<name>'
			name[0] = 'd';
			int j = 1;
			while(command[i] != '\0')
			{
				if(command[i] == ' ')
				{
					reply.status = FAILURE_INVALID;
					return reply;
				}
				name[j] = command[i];
				j++;
				i++;
			}
			name[j] = '\0';
			break;
		}

		//Lines 221-261 check for and handle user command JOIN
		i = 0;
		word = "JOIN";

		//Checks character by character if command == 'JOIN'
		for(i = 0; command[i] != '\0' || i < 4; i++)
		{
			if(word[i] != command[i])
				break;
		}

		//If i != 4, we know command was not 'JOIN'
		if(i == 4)
		{
			if(command[i] != '\0')
			{
				//Skip over the space character in command
				i++;
			}
			else
			{
				reply.status = FAILURE_INVALID;
				return reply;
			}

			//Format message to server in form: 'j<name>'
			name[0] = 'j';
			int j = 1;
			while(command[i] != '\0')
			{
				if(command[i] == ' ')
				{
					reply.status = FAILURE_INVALID;
					return reply;
				}
				name[j] = command[i];
				j++;
				i++;
			}
			name[j] = '\0';
			break;
		}

		//Lines 264-287 check for and handle user command LIST
		i = 0;
		word = "LIST";

		//Checks character by character if command == 'LIST'
		for(i = 0; command[i] != '\0' || i < 4; i++)
		{
			if(word[i] != command[i])
				break;
		}

		//LIST does not require additional arguments, if there are any
		//extra characters this is invalid formatting
		if(command[i] != '\0')
		{
			reply.status = FAILURE_INVALID;
			return reply;
		}
		else
		{
			//Message to Server is simply 'l'
			name[0] = 'l';
			name[1] = '\0';
			break;
		}
	}
	

	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------
	//Sends formatted message to server
	if(send(sockfd, name, sizeof(name), 0) == -1)
		perror("Send");
	
	//Receives response from server based on user message
	char buffer[256];
	if(recv(sockfd, buffer, sizeof(buffer), 0) == -1)
		perror("Client recv");
	reply = *(struct Reply *)buffer;

	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

	//Returns response from server
	return reply;
}

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------
	//Converts integer port to cstring
	char portStr[10];
	snprintf(portStr, 10, "%d", port);
	
	//Connects to given host and port
	int sockfd = connect_to(host, portStr);

	//Used for poll to allow proper chatroom functionality
	int pollVal;
	struct pollfd fds[2];
	int timeout;

	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	int fd = 0;
	char sendbuff[256];
	int bytes;
	while(1)
	{
		//Setting up file descriptor set to look for reads from sockfd
		fds[0].fd = sockfd;
		fds[0].events = 0;
		fds[0].events |= POLLIN;

		//Setting up file descriptor set to look for reads from STDIN
		fds[1].fd = fd;
		fds[1].events = 0;
		fds[1].events |= POLLIN;

		//Setting up poll timeout and polling file descriptors
		timeout = 2000;
		pollVal = poll(fds, 2, timeout);

		//File descriptors did not receive any reads
		if(pollVal == 0)
		{
			printf("");
		}
		else
		{
			//Checks if the sockfd received a read event
			if(fds[0].revents && POLLIN)
			{
				//Reads and displays response from server
				char displayBuffer[256];
				if(recv(sockfd, displayBuffer, sizeof(displayBuffer), 0) == -1)
					perror("Chatmode Recv");
				display_message(displayBuffer);

				//If message is the room closing, closes the socket file descriptor
				if(strcmp(displayBuffer, "Warning, chat room closed\n") == 0)
				{
					close(sockfd);
					break;
				}
			}
			else
			{
				//If STDIN received a read, takes this message and sends it to server
				memset((void *) sendbuff, 0, 256);
				bytes = read(fd, (void *) sendbuff, 255);
				if(send(sockfd, sendbuff, sizeof(sendbuff), 0) == -1)
					perror("Chatmode Send");
			}
		}
	}

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
}

