#include <glog/logging.h>
#include "test_server.h"
#include <boost/lexical_cast.hpp>

namespace sps {

stamp_t Server::handle_pull(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[PULL] pull req from "
            << "messenger " << msg->sender_messenger_id()
            << " node " << msg->sender_node_id();
    }
    auto p_ret_msg = std::make_shared<Message>();
    p_ret_msg->set_cmd("pull_ret");
    p_ret_msg->set_reply_timestamp(msg->timestamp());
    p_ret_msg->set_target_messenger_id(msg->sender_messenger_id());
    p_ret_msg->set_target_node_id(msg->sender_node_id());
    p_ret_msg->set_sender_node_id(node_id());
    p_ret_msg->mutable_fids()->Reserve((msg->fids()).size());
    for (auto fid: msg->fids()) {
        p_ret_msg->mutable_fids()->Add(fid);
        auto ret_vec = p_ret_msg->add_vecs();
        if (memory_.find(fid) == memory_.end()) {
            LOG(INFO) << "[PULL] fid empty";
            ret_vec->set_size(0);
        } else {
            ret_vec->set_size(memory_[fid].size());
            ret_vec->mutable_values()->Reserve(ret_vec->size());
            for (auto val: memory_[fid]) {
                ret_vec->mutable_values()->Add(val);
            }
        }
    }
    std::vector<msg_ptr> v{p_ret_msg};
    Messenger::get()->send(v);
    return 0;
}

stamp_t Server::handle_push(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[PUSH] push req from "
            << "messenger " << msg->sender_messenger_id()
            << " node " << msg->sender_node_id();
    }
    if (msg->fids_size() != msg->vecs_size()) {
        LOG(ERROR) << "[PUSH] msg->fids and vecs size not match";
        return -1;
    }
    for (int i = 0; i != msg->fids_size(); ++i) {
        if (memory_.find(msg->fids(i)) == memory_.end()) {
            std::vector<double> mem_vec;
            mem_vec.reserve(msg->vecs(i).size());
            for (auto i: msg->vecs(i).values()) {
                mem_vec.push_back(i);
            }
            memory_[msg->fids(i)] = std::move(mem_vec);
            std::string v;
        } else {
            if (memory_[msg->fids(i)].size() != msg->vecs(i).size()) {
                LOG(ERROR) << "[PUSH] msg fid " << msg->fids(i)
                        << " pushed vec and memory_ vec size not match";
                for (auto i:msg->vecs(i).values()) {
                    LOG(ERROR) << i;
                }
                continue;
            }
            auto &mem_vec = memory_[msg->fids(i)];
            for (int j = 0; j != msg->vecs(i).size(); ++j) {
                mem_vec[j] += msg->vecs(i).values(j);
            }
        }
    }
    if (FLAGS_debug) {
        for (auto iter = memory_.begin(); iter != memory_.end(); ++iter) {
            LOG(INFO) << "[PUSH]fid:" << iter->first;
            std::string v;
            for (auto i: iter->second) {
                v += ",";
                v += boost::lexical_cast<std::string>(i);
            }
            LOG(INFO) << "[PUSH]vec:" << v;
        }
    }
    return 0;
}

void Server::start(bool barrier) {
    LOG(INFO) << "server starting";
    handle_funcs_.emplace("pull", std::bind(&Server::handle_pull, this, std::placeholders::_1));
    handle_funcs_.emplace("push", std::bind(&Server::handle_push, this, std::placeholders::_1));
    std::thread t(&Server::node_thread, this);
    t.detach();
    Messenger::get()->add_node(this);
    LOG(INFO) << "server started";
    if (barrier) {
        //TODO barrier();
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
}

} //namespace

