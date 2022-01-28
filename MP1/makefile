# makefile

all: crc crsd

crc.o: crc.c interface.h
	gcc -g -w -c crc.c

crsd.o: crsc.cpp
	g++ -g -w -std=c++11 -c crsc.cpp -lpthread -lrt



common.o: common.h common.cpp
	g++ -g -w -std=c++11 -c common.cpp

RequestChannel.o: RequestChannel.h RequestChannel.cpp
	g++ -g -w -std=c++11 -c RequestChannel.cpp

MQRequestChannel.o: MQRequestChannel.h MQRequestChannel.cpp RequestChannel.o
	g++ -g -w -std=c++11 -c MQRequestChannel.cpp RequestChannel.o -lrt

FIFOreqchannel.o: FIFOreqchannel.h FIFOreqchannel.cpp RequestChannel.o
	g++ -g -w -std=c++11 -c FIFOreqchannel.cpp RequestChannel.o
	
SHMQueue.o: SHMQueue.h SHMQueue.cpp
	g++ -g -w -std=c++11 -c SHMQueue.cpp 

SHMRequestChannel.o: SHMRequestChannel.h SHMRequestChannel.cpp SHMQueue.o RequestChannel.o
	g++ -g -w -std=c++11 -c SHMRequestChannel.cpp RequestChannel.o SHMQueue.o

client: client.cpp FIFOreqchannel.o common.o RequestChannel.o MQRequestChannel.o SHMRequestChannel.o
	g++ -g -w -std=c++11 -o client client.cpp FIFOreqchannel.o common.o RequestChannel.o MQRequestChannel.o SHMRequestChannel.o -lpthread -lrt

server: server.cpp  FIFOreqchannel.o common.o RequestChannel.o MQRequestChannel.o
	g++ -g -w -std=c++11 -o server server.cpp FIFOreqchannel.o RequestChannel.o common.o MQRequestChannel.o SHMRequestChannel.o -lpthread -lrt

clean:
	rm -rf *.o *.csv fifo* server client data*_*