// chat_room.cpp: char room server

//#define DEBUG

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
//#include <boost/asio.hpp>
#include "chat_message.hpp"

//using boost::asio::ip::tcp;
//namespace asio = boost::asio;
#include "asio.hpp"
using asio::ip::tcp;

class chat_participant {
public:
    virtual ~chat_participant(){}
    virtual const char* id() const = 0;
    virtual void deliver(const chat_message& msg) = 0;
};

//typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

class chat_room {
public: 

    void join(chat_participant_ptr new_participant) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        std::cout << new_participant->id() << " joined the chat" << std::endl;

        participants_.insert(new_participant);

        // deliver recent messages to the new participants
        for (auto msg : recent_msg_)
            new_participant->deliver(msg);
        
        // deliever the messages that a new participant joined the chat
        chat_message msg;
        char admin_id[chat_message::id_length + 1] = "Admin";
        std::string admin_msg(new_participant->id());
        admin_msg += " joined the chat";

        msg.body_length(admin_msg.length() + chat_message::id_length);
        std::memcpy(msg.id(), admin_id, chat_message::id_length);
        std::memcpy(msg.msg(), admin_msg.c_str(), admin_msg.length());
        msg.encode_header();

        deliver(msg);
    }

    void leave(chat_participant_ptr participant) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        std::cout << participant->id() << " left the chat" << std::endl;

        participants_.erase(participant);

        // deliever the messages that a participant left the chat
        chat_message msg;
        char admin_id[chat_message::id_length + 1] = "Admin";
        std::string admin_msg(participant->id());
        admin_msg += " left the chat";

        msg.body_length(admin_msg.length() + chat_message::id_length);
        std::memcpy(msg.id(), admin_id, chat_message::id_length);
        std::memcpy(msg.msg(), admin_msg.c_str(), admin_msg.length());
        msg.encode_header();

        deliver(msg);
    }

    void deliver(const chat_message& msg) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        // push the new message into the queue
        recent_msg_.push_back(msg);
        while (recent_msg_.size() > max_recent_msg) recent_msg_.pop_front();

        // deliver the new message to all the participants
        for (auto participant : participants_)
            if (std::strncmp(msg.id(), participant->id(), chat_message::id_length) != 0)
                participant->deliver(msg);
            else {
                //std::cout.write(msg.id(), chat_message::id_length);
                //std::cout << "\n";
                //std::cout.write(participant->id(), chat_message::id_length);
            }
    }

private:
    std::set<chat_participant_ptr> participants_;
    enum { max_recent_msg = 100 };
    std::deque<chat_message> recent_msg_;
};

class chat_session : 
    public chat_participant,
    public std::enable_shared_from_this<chat_session> {

public: 
    chat_session(asio::io_context& io_context, chat_room& room) :
        socket_(io_context),
        room_(room) {
    }

    tcp::socket& socket() {
        return socket_;
    }

    void wait_for_id() {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        asio::async_read(socket_,
            asio::buffer(id_, chat_message::id_length),
            std::bind(&chat_session::start, 
                shared_from_this(),  
                //boost::asio::placeholders::error));
                std::placeholders::_1));
    }
    
    void start(const std::error_code& error) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif


        room_.join(shared_from_this());
        // read the header from read_msg_ first
        // invoke handle_read_header and trigger the body reading event
        asio::async_read(socket_,
            asio::buffer(read_msg_.data(), chat_message::header_length),
            std::bind(&chat_session::handle_read_header, 
                shared_from_this(),  
                //boost::asio::placeholders::error));
                std::placeholders::_1));
    }

    void handle_read_header(const std::error_code& error) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        if (!error && read_msg_.decode_header()) {
            //std::cout << id_ << " sends message with length:" << read_msg_.body_length() << std::endl;
            asio::async_read(socket_, 
                asio::buffer(read_msg_.body(), read_msg_.body_length()), 
                bind(&chat_session::handle_read_body,
                    shared_from_this(),
                    //boost::asio::placeholders::error));
                    std::placeholders::_1));
        } else {
            room_.leave(shared_from_this());
        }
    }

    void handle_read_body(const std::error_code& error) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        if (!error) {
            std::cout << id_ << " says: ";
            std::cout.write(read_msg_.msg(), read_msg_.body_length() - chat_message::id_length);
            std::cout << std::endl;

            room_.deliver(read_msg_);

            // wait for the next message
            asio::async_read(socket_,
                asio::buffer(read_msg_.data(), chat_message::header_length),
                std::bind(&chat_session::handle_read_header, 
                    shared_from_this(),  
                    //boost::asio::placeholders::error));
                    std::placeholders::_1));
        } else {
            room_.leave(shared_from_this());
        }
    }

    void deliver(const chat_message& msg) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            chat_message& write_msg = write_msgs_.front();
            asio::async_write(socket_, 
                asio::buffer(write_msg.data(), write_msg.length()), 
                std::bind(&chat_session::handle_write,
                    shared_from_this(),
                    //boost::asio::placeholders::error));
                    std::placeholders::_1));
        }
    }

    void handle_write(const std::error_code& error) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif

        if (!error) {
            write_msgs_.pop_front();

            if (!write_msgs_.empty()) {
                //iteratively call itself, until no message in the queue
                chat_message& write_msg = write_msgs_.front();
                asio::async_write(socket_, 
                    asio::buffer(write_msg.data(), write_msg.length()), 
                    std::bind(&chat_session::handle_write,
                        shared_from_this(),
                        //boost::asio::placeholders::error));
                        std::placeholders::_1));
            }
        } else {
            room_.leave(shared_from_this());
        }
    }

    const char* id() const{
        return id_;
    }

private:
    tcp::socket socket_;
    chat_room& room_;
    chat_message read_msg_;
    std::deque<chat_message> write_msgs_;
    char id_[chat_message::id_length + 1];
};

typedef std::shared_ptr<chat_session> chat_session_ptr;

// ------------------------------------------------------

class chat_server {
public:
    chat_server(asio::io_context& io_context, tcp::endpoint& endpoint) : 
        io_context_(io_context), 
        acceptor_(io_context, endpoint) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif
        
        chat_session_ptr session(new chat_session(io_context_, room_));
        acceptor_.async_accept(session->socket(), 
            std::bind(&chat_server::handle_accept, this, session, 
                //boost::asio::placeholders::error));
                std::placeholders::_1));
    }

    void handle_accept(chat_session_ptr session, 
        const std::error_code &error) {
        #ifdef DEBUG
        std::cout << __FUNCTION__ << std::endl;
        #endif
        
        if (!error) {
            session->wait_for_id();
            chat_session_ptr new_session(new chat_session(io_context_, room_));
            acceptor_.async_accept(new_session->socket(), 
                std::bind(&chat_server::handle_accept, this, new_session, 
                    //boost::asio::placeholders::error));
                    std::placeholders::_1));
        } 
    }

private:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    chat_room room_;

};

typedef std::shared_ptr<chat_server> chat_server_ptr;

int main(int argc, char* argv[]) {

    try {
        asio::io_context io_context;
        tcp::endpoint endpoint(tcp::v4(), 1000);
        chat_server_ptr server(new chat_server(io_context, endpoint));
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

