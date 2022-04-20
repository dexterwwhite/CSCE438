#Start up Coordinator
./coordinator -p 1234 &

#Start up server cluster 1
./tsd -h localhost -c 1234 -p 1235 -i 1 -t master &
./tsd -h localhost -c 1234 -p 1236 -i 1 -t slave &
./synchronizer -h localhost -c 1234 -p 1237 -i 1 &

#Start up server cluster 2
./tsd -h localhost -c 1234 -p 1238 -i 2 -t master &
./tsd -h localhost -c 1234 -p 1239 -i 2 -t slave &
./synchronizer -h localhost -c 1234 -p 1240 -i 2 &

#Start up server cluster 3
./tsd -h localhost -c 1234 -p 1241 -i 3 -t master &
./tsd -h localhost -c 1234 -p 1242 -i 3 -t slave &
./synchronizer -h localhost -c 1234 -p 1243 -i 3 &
