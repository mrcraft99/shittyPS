#pragma once
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <memory>
#include <gflags/gflags.h>
#include "util.h"
#include "message.pb.h"

DECLARE_int32(port);
DECLARE_bool(debug_messenger);

namespace sps {
using namespace boost::asio;
class Session;
class Node;

class Messenger {
public:
    friend class Session;
    static std::shared_ptr<Messenger> get() {
        static auto e = std::shared_ptr<Messenger>(new Messenger());
        return e;
    }
    void send(std::vector<msg_ptr> &msgs);
    //void process_msg(const Message &msg);
    int64_t messenger_id() { return messenger_id_; }
    void stop();
    void start();
    void add_node(Node* node);
    stamp_t gen_stamp() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

private:
    Messenger() : acceptor_(messenger_service_,
                            ip::tcp::endpoint(ip::tcp::v4(), FLAGS_port)) {
        socket_base::reuse_address option(true);
        acceptor_.set_option(option);
    }
    void init_environment();
    void connect_server(msg_ptr msg);
    void connect_scheduler();
    void register_messenger();
    void dispatch_msg(const msg_ptr msg, std::shared_ptr<Session> s);
    void on_accept(const boost::system::error_code &err, ip::tcp::socket socket_);

    io_service messenger_service_;
    std::unordered_map<int, Node*> node_id2p_node_;
    int64_t messenger_id_ = -1;
    // avoid repeated init
    std::mutex msg_mutex_;
    std::mutex start_mutex_;
    int start_stage_ = 0;
    std::condition_variable start_condvar_;
    std::unordered_map<int, std::shared_ptr<Session>> messenger_id2session_;
    ip::tcp::acceptor acceptor_;
};


class Session : public std::enable_shared_from_this<Session> {
public:
    Session(ip::tcp::socket sock) : socket_(std::move(sock)) { };
    void start() {
        auto self(shared_from_this());
        if (FLAGS_debug_messenger) {
            LOG(INFO) << "session start";
        }
        socket_.async_read_some(buffer(buf_, MESSAGE_MAX_SIZE),
                std::bind(&Session::on_read, self, std::placeholders::_1, std::placeholders::_2));
    }
    void async_write_msg(Message& msg) {
        std::string msg_str;
        msg.SerializeToString(&msg_str);
        if (FLAGS_debug_messenger) {
            LOG(INFO) << "sending: " << msg_str.size() << " bytes async";
        }
        if (msg_str.size() > MESSAGE_MAX_SIZE) {
            //TODO devide
        }
        socket_.async_write_some(buffer(msg_str.c_str(), msg_str.size()),
                std::bind(&Session::on_write, this, std::placeholders::_1, std::placeholders::_2));
    }
private:
    void on_read(boost::system::error_code err, size_t bytes) {
        auto self(shared_from_this());
        if (FLAGS_debug_messenger) {
            LOG(INFO) << "recving msg";
        }
        if (!err) {
            auto msg = std::make_shared<Message>();
            msg->ParseFromArray(buf_, bytes);
            memset(buf_, 0, MESSAGE_MAX_SIZE);
            Messenger::get()->dispatch_msg(msg, self);
            socket_.async_read_some(buffer(buf_, MESSAGE_MAX_SIZE),
                    std::bind(&Session::on_read, self, std::placeholders::_1, std::placeholders::_2));
        } else {
            // TODO shutdown
            LOG(ERROR) << err;
        }

    }

    void on_write(const boost::system::error_code &err, size_t bytes) {
        return;
    }

    char buf_[MESSAGE_MAX_SIZE];
    ip::tcp::socket socket_;
};
} //namespace sps
