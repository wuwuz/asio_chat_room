#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include <cstdio>
#include <cstring>
#include <cstdlib>

class chat_message {

public : 
    enum { header_length = 4 }; // 4 digit header
    enum { id_length = 8; } // 4 digit id_length
    enum { max_body_length = 1024 }; // 1KB body

    chat_message() : body_length_(0) {
    }

    const char* data() const {
        return data_;
    }

    char* data() {
        return data_;
    }

    std::size_t length() {
        return body_length_ + header_length;
    }

    
    //const char* id() const {
        //return data_ + header_length;
    //}

    //char* id() {
        //return data_ + header_length;
    //}

    const char* body() const {
        return data_ + header_length ;
    }

    char* body() {
        return data_ + header_length;
    }

    std::size_t body_length() {
        return body_length_;
    }

    void body_length(std::size_t new_length) { 
        body_length_ = new_length;
        if (body_length_ > max_body_length) 
            body_length_ = max_body_length;
        //body_length = std::max(new_length, max_body_length);
    }

    bool decode_header() {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length) 
            body_length_ = max_body_length;
        return true;
    }

    bool encode_header() { 
        char header[header_length + 1] = "";
        sprintf(header, "%4d", static_cast<int>(body_length_));
        std::memcpy(data_, header, header_length);
        return true;
    }

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};

#endif