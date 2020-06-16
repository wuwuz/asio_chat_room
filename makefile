
CFLAGS = -pg -g -Wall -std=c++11 -pthread -DASIO_STANDALONE -I ./asio/include

all: chat_server chat_client

chat_server: chat_server.cpp chat_message.hpp
	g++ $(CFLAGS) -o chat_server chat_server.cpp
	
chat_client: chat_client.cpp chat_message.hpp
	g++ $(CFLAGS) -o chat_client chat_client.cpp
	

clean:
	rm -f chat_server
	rm -f chat_client
