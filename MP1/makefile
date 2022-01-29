# makefile

all: crc crsd

crc: crc.c interface.h
	gcc -g -w -o crc crc.c 

crsd: crsd.cpp interface.h
	g++ -std=c++11 -g -w -o crsd crsd.cpp -lpthread -lrt

clean:
	rm -rf *.o *.csv fifo* crc crsd data*_*