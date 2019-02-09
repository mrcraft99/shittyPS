#include <iostream>
#include <glog/logging.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "test_worker.h"
//#include "test_scheduler.h"

DEFINE_string(ip, "127.0.0.1", "local ip");
DEFINE_int32(port, 9992, "node port");
DEFINE_string(scheduler_ip, "127.0.0.1", "scheduler ip");
DEFINE_int32(scheduler_port, 9990, "scheduler port");
DEFINE_int32(k_messenger_threads, 1, "num of recv threads");
DEFINE_string(role, "worker", "worker/server/scheduler");
DEFINE_bool(debug_messenger, false, "debug messenger");

namespace sps {
using namespace boost::asio;
using boost::lexical_cast;
using namespace std::placeholders;


void Messenger::send(std::vector<msg_ptr> &msgs) {
    for (auto &msg: msgs) {
        if (msg->target_messenger_id() < 0) {
            auto info = msg->mutable_cmd_info();
            int real_messenger_id = lexical_cast<msnger_id_t>((*info)["messenger_id"]);
            LOG(INFO) << "target: " << msg->target_messenger_id()
                << " ,change to:" << real_messenger_id;
            {
                std::lock_guard<std::mutex> lk(msg_mutex_);
                messenger_id2session_[real_messenger_id] = messenger_id2session_[msg->target_messenger_id()];
                messenger_id2session_.erase(msg->target_messenger_id());
            }
            msg->set_target_messenger_id(real_messenger_id);
        }
        //TODO scheduler asign id
        msg->set_sender_messenger_id(messenger_id_);
        if (messenger_id2session_.find(msg->target_messenger_id()) != messenger_id2session_.end()) {
            messenger_id2session_[msg->target_messenger_id()]->async_write_msg(*msg);
            if (FLAGS_debug_messenger) {
                LOG(INFO) << "sending to " << msg->target_messenger_id();
            }
        } else {
            //TODO
            if (FLAGS_debug_messenger) {
                LOG(INFO) << "???";
                LOG(INFO) << msg->target_messenger_id();
                LOG(INFO) << msg->cmd();
            }
        }
    }
}

void Messenger::dispatch_msg(const msg_ptr msg, std::shared_ptr<Session> s) {
    if (msg->sender_messenger_id() < 0) {
        //register;
        msnger_id_t i = 0;
        std::lock_guard<std::mutex> lk(msg_mutex_);
        while (messenger_id2session_.find(--i) != messenger_id2session_.end());
        msg->set_sender_messenger_id(i);
    } else if (msg->sender_messenger_id() == SCHEDULER) {
        //TODO move to test
        if (msg->cmd() == "register_ret") {
            messenger_id_ = lexical_cast<msnger_id_t>((*(msg->mutable_cmd_info()))["messenger_id"]);
            if (FLAGS_debug_messenger) {
                LOG(INFO) << "asigned messenger id: " << messenger_id_;
            }
            if (FLAGS_role == "worker") {
                connect_server(msg);
            }
            start_condvar_.notify_one();
            return;
        } else {
        } 
    }
    if (messenger_id2session_.find(msg->sender_messenger_id()) == messenger_id2session_.end()) {
        std::lock_guard<std::mutex> lk(msg_mutex_);
        messenger_id2session_[msg->sender_messenger_id()] = s;
    }
    auto p_node = node_id2p_node_[msg->target_node_id()];
    std::lock_guard<std::mutex> lk(p_node->recv_mutex_);
    if (FLAGS_debug_messenger) {
        LOG(INFO) << "dispatch_msg from : " << msg->sender_messenger_id();
    }
    p_node->mailbox.push_back(std::move(msg));
    p_node->recv_condvar_.notify_one();
}

void Messenger::connect_scheduler() {
    ip::tcp::endpoint scheduler_ep(ip::address::from_string(FLAGS_scheduler_ip), FLAGS_scheduler_port);
    ip::tcp::socket socket(messenger_service_);
    LOG(INFO) << "connecting scheduler";
    socket.connect(scheduler_ep);
    LOG(INFO) << "scheduler connected";
    auto s = std::make_shared<Session>(std::move(socket));
    messenger_id2session_[SCHEDULER] = s;
    s->start();
    register_messenger();
}

void Messenger::register_messenger() {
    auto p_req_msg = std::make_shared<Message>();
    p_req_msg->set_cmd("register");
    auto info_map = p_req_msg->mutable_cmd_info();
    (*info_map)["ip"] = FLAGS_ip;
    (*info_map)["port"] = lexical_cast<std::string>(FLAGS_port);
    (*info_map)["role"] = FLAGS_role;
    p_req_msg->set_target_messenger_id(SCHEDULER);
    std::vector<msg_ptr> v{p_req_msg};
    send(v);
}

void Messenger::add_node(Node* node) {
    node_id2p_node_[node->node_id()] = node;
    if (FLAGS_role == "worker") {
        auto p_req_msg = std::make_shared<Message>();
        p_req_msg->set_job_name(dynamic_cast<Worker*>(node)->job_name());
        p_req_msg->set_cmd("register_node");
        p_req_msg->set_target_messenger_id(SCHEDULER);
        p_req_msg->set_sender_node_id(node->node_id());
        std::vector<msg_ptr> v{p_req_msg};
        send(v);
    }
}

void Messenger::connect_server(msg_ptr msg) {
    auto m = *(msg->mutable_cmd_info());
    std::string server_ips = trim_copy_if(m["server_ips"], boost::is_any_of(","));
    std::string server_ports = trim_copy_if(m["server_ports"], boost::is_any_of(","));
    std::string messenger_ids = trim_copy_if(m["server_ids"], boost::is_any_of(","));
    std::vector<std::string> server_ip_list;
    std::vector<std::string> server_port_list;
    std::vector<std::string> messenger_id_list;
    boost::split(server_ip_list, server_ips, boost::is_any_of(","), boost::token_compress_on);
    boost::split(server_port_list, server_ports, boost::is_any_of(","), boost::token_compress_on);
    boost::split(messenger_id_list, messenger_ids, boost::is_any_of(","), boost::token_compress_on);
    for (int i = 0; i != server_ip_list.size(); ++i) {
        ip::tcp::endpoint server_ep(ip::address::from_string(server_ip_list[i]),
                lexical_cast<int32_t>(server_port_list[i]));
        auto p_socket = std::make_shared<ip::tcp::socket>(messenger_service_);
        LOG(INFO) << "connecting server" << server_ip_list[i];
        p_socket->connect(server_ep);
        LOG(INFO) << "server " << server_ip_list[i] << " connected";
        auto s = std::make_shared<Session>(std::move(*p_socket));
        s->start();
        auto smid = lexical_cast<msnger_id_t>(messenger_id_list[i]);
        messenger_id2session_[smid] = s;
        Worker::server_list.push_back(smid);
    }
}

void Messenger::on_accept(const boost::system::error_code &err, ip::tcp::socket socket_) {
    if (!err) {
        if (FLAGS_debug_messenger) {
            LOG(INFO) << "accepting";
        }
        std::make_shared<Session>(std::move(socket_))->start();
        acceptor_.async_accept(std::bind(&Messenger::on_accept, this,
                    std::placeholders::_1, std::placeholders::_2));
    } else {
        if (FLAGS_debug_messenger) {
            LOG(INFO) << err.message();
        }
        //TODO
    }
}

void Messenger::start() {
    std::lock_guard<std::mutex> lk(start_mutex_);
    if (start_stage_ == 0) {
        start_stage_ = 1;
        if (FLAGS_role == "worker") {
            connect_scheduler();
        }
        else if (FLAGS_role == "server") {
            connect_scheduler();
            acceptor_.async_accept(std::bind(&Messenger::on_accept, this,
                        std::placeholders::_1, std::placeholders::_2));
        } else if (FLAGS_role == "scheduler") {
            messenger_id_ = SCHEDULER;
            acceptor_.async_accept(std::bind(&Messenger::on_accept, this,
                        std::placeholders::_1, std::placeholders::_2));
        } else {
            LOG(ERROR) << "invalid role";
            exit(1);
        }
        for (size_t i = 0; i != FLAGS_k_messenger_threads; ++i) {
            std::thread t([&](){messenger_service_.run();});
            t.detach();
        }
        if (FLAGS_role != "scheduler") {
            std::unique_lock<std::mutex> wait_lock(msg_mutex_);
            start_condvar_.wait(wait_lock, [this]{
                return messenger_id_ > 0;
            });
            LOG(INFO) << "msnger id: " << messenger_id_;
        }
    }
}

void Messenger::stop() {
    return;
}

} //namespace sps
