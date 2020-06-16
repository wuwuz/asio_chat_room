// chat_room.cpp: char room server

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

class chat_participant {
public:
    virtual ~chat_participant(){}
    virtual void deliver(const chat_message& msg) = 0;
};

//typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

class chat_room {
public: 

    void join(chat_participant_ptr new_participant) {
        std::cout << __FUNCTION__ << std::endl;
        participants_.insert(new_participant);

        // deliver recent messages to the new participants
        for (auto msg : recent_msg_)
            new_participant->deliver(msg);
    }

    void leave(chat_participant_ptr participant) {
        std::cout << __FUNCTION__ << std::endl;
        participants_.erase(participant);
    }

    void deliver(const chat_message& msg) {
        std::cout << __FUNCTION__ << std::endl;
        // push the new message into the queue
        recent_msg_.push_back(msg);
        while (recent_msg_.size() > max_recent_msg) recent_msg_.pop_front();

        // deliver the new message to all the participants
        for (auto participant : participants_)
            participant->deliver(msg);
    }

private:
    std::set<chat_participant_ptr> participants_;
    enum { max_recent_msg = 100 };
    std::deque<chat_message> recent_msg_;
};

class chat_session : 
    public chat_participant,
    public boost::enable_shared_from_this<chat_session> {

public: 
    chat_session(boost::asio::io_context& io_context, chat_room& room) :
        socket_(io_context),
        room_(room) {
    }

    tcp::socket& socket() {
        return socket_;
    }

    void start() {
        std::cout << __FUNCTION__ << std::endl;
        std::stringstream ss;
        ss << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port();
        id_ = ss.str();
        std::cout << id_ << " connected" << std::endl;

        room_.join(shared_from_this());
        // read the header from read_msg_ first
        // invoke handle_read_header and trigger the body reading event
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length),
            boost::bind(&chat_session::handle_read_header, 
                shared_from_this(),  
                boost::asio::placeholders::error));
    }

    void handle_read_header(const boost::system::error_code& error) {
        std::cout << __FUNCTION__ << std::endl;
        if (!error && read_msg_.decode_header()) {
            std::cout << id_ << " sends message with length:" << read_msg_.body_length() << std::endl;
            boost::asio::async_read(socket_, 
                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()), 
                boost::bind(&chat_session::handle_read_body,
                    shared_from_this(),
                    boost::asio::placeholders::error));
        } else {
            room_.leave(shared_from_this());
        }
    }

    void handle_read_body(const boost::system::error_code& error) {
        std::cout << __FUNCTION__ << std::endl;
        if (!error) {
            std::cout << id_ << " sends message:";
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << std::endl;

            room_.deliver(read_msg_);

            // wait for the next message
            boost::asio::async_read(socket_,
                boost::asio::buffer(read_msg_.data(), chat_message::header_length),
                boost::bind(&chat_session::handle_read_header, 
                    shared_from_this(),  
                    boost::asio::placeholders::error));
        } else {
            room_.leave(shared_from_this());
        }
    }

    void deliver(const chat_message& msg) {
        std::cout << __FUNCTION__ << std::endl;
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            chat_message& write_msg = write_msgs_.front();
            boost::asio::async_write(socket_, 
                boost::asio::buffer(write_msg.data(), write_msg.length()), 
                boost::bind(&chat_session::handle_write,
                    shared_from_this(),
                    boost::asio::placeholders::error));
        }
    }

    void handle_write(const boost::system::error_code& error) {
        std::cout << __FUNCTION__ << std::endl;
        if (!error) {
            write_msgs_.pop_front();

            if (!write_msgs_.empty()) {
                //iteratively call itself, until no message in the queue
                chat_message& write_msg = write_msgs_.front();
                boost::asio::async_write(socket_, 
                    boost::asio::buffer(write_msg.data(), write_msg.length()), 
                    boost::bind(&chat_session::handle_write,
                        shared_from_this(),
                        boost::asio::placeholders::error));
            }
        } else {
            room_.leave(shared_from_this());
        }
    }

private:
    tcp::socket socket_;
    chat_room& room_;
    chat_message read_msg_;
    std::deque<chat_message> write_msgs_;
    std::string id_;
};

typedef boost::shared_ptr<chat_session> chat_session_ptr;

// ------------------------------------------------------

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context, tcp::endpoint& endpoint) : 
        io_context_(io_context), 
        acceptor_(io_context, endpoint) {
        std::cout << __FUNCTION__ << std::endl;
        
        chat_session_ptr session(new chat_session(io_context_, room_));
        acceptor_.async_accept(session->socket(), 
            boost::bind(&chat_server::handle_accept, this, session, 
                boost::asio::placeholders::error));
    }

    void handle_accept(chat_session_ptr session, 
        const boost::system::error_code &error) {
        std::cout << __FUNCTION__ << std::endl;
        
        if (!error) {
            session->start();
            chat_session_ptr new_session(new chat_session(io_context_, room_));
            acceptor_.async_accept(new_session->socket(), 
                boost::bind(&chat_server::handle_accept, this, new_session, 
                    boost::asio::placeholders::error));
        } 
    }

private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    chat_room room_;

};

typedef boost::shared_ptr<chat_server> chat_server_ptr;

int main(int argc, char* argv[]) {

    try {
        boost::asio::io_context io_context;
        tcp::endpoint endpoint(tcp::v4(), 1000);
        chat_server_ptr server(new chat_server(io_context, endpoint));
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

