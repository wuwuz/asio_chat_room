// chat_client.cpp : chat room client

//#define DEBUG

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

class chat_client {
public:
    chat_client(boost::asio::io_context& io_context, 
        tcp::resolver::results_type& endpoints,
        char* id) :
        io_context_(io_context), 
        socket_(io_context){

        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        std::memcpy(id_, id, chat_message::id_length);
        
        do_connect(endpoints);
    }

    void write(chat_message& msg) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        auto f = [this, msg]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if (!write_in_progress) {
                do_write();
            }
        };
        //f();
        boost::asio::post(io_context_, f);
    }

    void close() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        auto f = [this]() {
            socket_.close();
        };
        //f();
        boost::asio::post(io_context_, f);
    }
    
private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    chat_message read_msg_;
    std::deque<chat_message> write_msgs_;
    char id_[chat_message::id_length + 1];

    void do_connect(const tcp::resolver::results_type& endpoints) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        boost::asio::async_connect(socket_, endpoints, 
            [this](boost::system::error_code error, tcp::endpoint) {
                if (!error) {
                    //do_read_header();
                    send_id();
                }
            });
    }

    void send_id() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        boost::asio::async_write(socket_, 
            boost::asio::buffer(id_, chat_message::id_length), 
            [this](boost::system::error_code error, std::size_t /*length*/) {
                if (!error) {
                    do_read_header();
                }
            });
    }

    void do_read_header() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        boost::asio::async_read(socket_, 
            boost::asio::buffer(read_msg_.data(), chat_message::header_length), 
            [this](boost::system::error_code error, std::size_t /*length*/) {
                if (!error && read_msg_.decode_header()) {
                    do_read_body();
                } else {
                    socket_.close();
                }
            });
    }

    void do_read_body() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()), 
            [this](boost::system::error_code error, std::size_t /*length*/){
                if (!error) {
                    std::cout.write(read_msg_.id(), chat_message::id_length);
                    std::cout << " says: ";
                    std::cout.write(read_msg_.msg(), read_msg_.body_length() - chat_message::id_length);
                    std::cout << "\n";
                    do_read_header();
                } else {
                    socket_.close();
                }
            });
    }

    void do_write() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        chat_message& msg = write_msgs_.front();
        boost::asio::async_write(socket_, 
            boost::asio::buffer(msg.data(), msg.length()), 
            [this](boost::system::error_code error, std::size_t /*length*/){
                if (!error) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) {
                        do_write();
                    }
                } else {
                    socket_.close();
                }
            });
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: chat_client <host> <port> <userid>" << std::endl;
        return 1;
    }

    if (std::strlen(argv[3]) > chat_message::id_length) {
        std::cerr << "Error: userid's length exceeds " << chat_message::id_length << std::endl;
        return 1;
    }

    char id[chat_message::id_length + 1] = "";
    std::memcpy(id, argv[3], std::strlen(argv[3]));

    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        chat_client client(io_context, endpoints, id);
        std::thread t([&io_context]() {
            io_context.run();
            });

        char line[chat_message::max_body_length + 1];
        while (std::cin.getline(line, chat_message::max_body_length + 1)) {
            chat_message msg;
            std::size_t len = std::strlen(line);
            msg.body_length(len + chat_message::id_length);
            std::memcpy(msg.id(), id, chat_message::id_length);
            std::memcpy(msg.msg(), line, len);
            msg.encode_header();
            client.write(msg);
        }

        client.close();
        t.join();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }


    return 0;
}

