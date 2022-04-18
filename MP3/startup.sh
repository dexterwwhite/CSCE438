#Start up Coordinator
./coordinator -p 1234 &

#Start up server cluster 1
./tsd -h localhost -c 1234 -p 1236 -i 1 -t slave &
./tsd -h localhost -c 1234 -p 1235 -i 1 -t master &
#./synchronizer

#Start up server cluster 2
./tsd -h localhost -c 1234 -p 1238 -i 2 -t slave &
./tsd -h localhost -c 1234 -p 1237 -i 2 -t master &
#./synchronizer

#Start up server cluster 3
./tsd -h localhost -c 1234 -p 1240 -i 3 -t slave &
./tsd -h localhost -c 1234 -p 1239 -i 3 -t master &
#./synchronizer
