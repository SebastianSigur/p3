all: tsamgroup82 client
	
tsamgroup82: tsamgroup82.cpp
	g++ -std=c++11 -lpthread tsamgroup82.cpp -o tsamgroup82

client: client.cpp
	g++ -std=c++11 -lpthread client.cpp -o client
