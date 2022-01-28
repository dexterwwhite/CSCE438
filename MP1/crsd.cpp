#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

using std::cout, std::endl;

int main(int argc, char** argv)
{
    if(argc != 2 || argc != 3)
    {
        cout << "Incorrect Command Line Arguments!" << endl;
        exit(1);
    }

    char* this_port = "1500\0";

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

    if(getaddrinfo(NULL, this_port, &hints, &res) == -1)
        perror("Getaddrinfo error: ");

    // make a socket, bind it, and listen on it:

    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        perror("socket error: ");
    
    if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("bind error: ");
    }
    
    if(listen(sockfd, 10) == -1)
    {
        close(sockfd);
        perror("listen error: ");
    }

    // now accept an incoming connection:

    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    // ready to communicate on socket descriptor new_fd!
}