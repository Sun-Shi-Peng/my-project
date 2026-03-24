all :client6
client2:client6.cpp
	g++ -std=c++11 -o $@ $^ -lpthread
clean:
	rm -f client6

client:tcp_cli.cc
	g++ -std=c++11 -o $@ $^ -lpthread
tcp_server:tcp_srv.cc
	g++ -std=c++11 -o $@ $^ -lpthread
server:server.cc
	g++ -std=c++11 -o $@ $^ -lpthread