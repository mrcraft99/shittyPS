#include <glog/logging.h>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include "test_scheduler.h"

namespace sps {

stamp_t Scheduler::handle_batch_done(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[BATCH] batch_done msg from "
            << msg->sender_messenger_id() << " node: "
            << msg->sender_node_id();
    }
    msnger_id_t mid = msg->sender_messenger_id();
    int nid = msg->sender_node_id();
    int mnid = get_mnid(mid, nid);
    auto &step2mnids = job2step2mnid_set_[msg->job_name()];
    std::lock_guard<std::mutex> sync_lock(sync_mutex_);
    int step;
    if (FLAGS_debug) {
        for (auto step: step2mnids) {
            LOG(INFO) << step.first;
            for (auto it: step.second) {
                LOG(INFO) << it;
            }
        }
    }
    for (auto pair: step2mnids) {
        if (pair.second.find(mnid) != pair.second.end()) {
            step = pair.first;
            step2mnids[step].erase(mnid);
            break;
        }
    }
    if (step == step2mnids.begin()->first && step2mnids[step].empty()) {
        Messenger::get()->send(ready_msgs_);
        ready_msgs_.clear();
        step2mnids.erase(step);
    }

    step2mnids[step+1].insert(mnid);
    if (FLAGS_debug) {
        for (auto step: step2mnids) {
            LOG(INFO) << step.first;
            for (auto it: step.second) {
                LOG(INFO) << it;
            }
        }
    }
    return 0;
}

stamp_t Scheduler::handle_sync(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[SYNC] req from mid: "
            << msg->sender_messenger_id() << "nid: "
            << msg->sender_node_id();
    }
    auto p_ret_msg = std::make_shared<Message>();
    msnger_id_t mid = msg->sender_messenger_id();
    int nid = msg->sender_node_id();
    int mnid = get_mnid(mid, nid);
    p_ret_msg->set_target_messenger_id(mid);
    p_ret_msg->set_sender_node_id(node_id());
    p_ret_msg->set_target_node_id(nid);
    p_ret_msg->set_reply_timestamp(msg->timestamp());
    p_ret_msg->set_cmd("ready");
    auto req_info = msg->mutable_cmd_info();
    std::lock_guard<std::mutex> sync_lock(sync_mutex_);
    auto &step2mnids = job2step2mnid_set_[msg->job_name()];
    auto max_iter = step2mnids.rbegin();
    auto min_iter = step2mnids.begin();
    if (FLAGS_debug) {
        for (auto step: step2mnids) {
            LOG(INFO) << step.first;
            for (auto it: step.second) {
                LOG(INFO) << it;
            }
        }
    }
    if ((*max_iter).second.find(mnid) != (*max_iter).second.end()) {
        if ((*max_iter).first - min_iter->first <= boost::lexical_cast<int>((*req_info)["delay"])) {
            std::vector<msg_ptr> v{p_ret_msg};
            Messenger::get()->send(v);
            LOG(INFO) << "00";
        } else if (min_iter->second.empty()) {
            ready_msgs_.push_back(p_ret_msg);
            Messenger::get()->send(ready_msgs_);
            ready_msgs_.clear();
            step2mnids.erase(min_iter->first);
            LOG(INFO) << "01";
        } else {
            ready_msgs_.push_back(p_ret_msg);
            LOG(INFO) << "02";
        }
    } else {
        std::vector<msg_ptr> v{p_ret_msg};
        Messenger::get()->send(v);
        LOG(INFO) << "03";
    }
    if (FLAGS_debug) {
        for (auto step: step2mnids) {
            LOG(INFO) << step.first;
            for (auto it: step.second) {
                LOG(INFO) << it;
            }
        }
    }
    return 0;
}

stamp_t Scheduler::handle_register(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[REGISTER] register req from "
            << msg->sender_messenger_id();
    }
    msnger_id_t mid = msg->sender_messenger_id();
    auto req_info = msg->mutable_cmd_info();
    auto p_ret_msg = std::make_shared<Message>();
    p_ret_msg->set_cmd("register_ret");
    auto ret_info = p_ret_msg->mutable_cmd_info();
    if ((*req_info)["role"] == "server") {
        if (server_messenger_ids_.empty()) {
            server_messenger_ids_.push_back(1);
        } else {
            msnger_id_t id = server_messenger_ids_[0] + 2;
            server_messenger_ids_.push_back(id);
        }
        std::make_heap(server_messenger_ids_.begin(), server_messenger_ids_.end());
        (*ret_info)["messenger_id"] = boost::lexical_cast<std::string>(server_messenger_ids_[0]);
        messenger_id2ip_port_[server_messenger_ids_[0]] =
            std::pair<std::string, std::string>((*req_info)["ip"], (*req_info)["port"]);
    } else if ((*req_info)["role"] == "worker") {
        if (worker_messenger_ids_.empty()) {
            worker_messenger_ids_.push_back(2);
        } else {
            int id = worker_messenger_ids_[0] + 2;
            worker_messenger_ids_.push_back(id);
        }
        std::make_heap(worker_messenger_ids_.begin(), worker_messenger_ids_.end());
        (*ret_info)["messenger_id"] = boost::lexical_cast<std::string>(worker_messenger_ids_[0]);
        for (auto messenger_id: server_messenger_ids_) {
            (*ret_info)["server_ids"] += boost::lexical_cast<std::string>(messenger_id);
            (*ret_info)["server_ids"] += ",";
            auto &ip_port = messenger_id2ip_port_[messenger_id];
                (*ret_info)["server_ips"] += ip_port.first;
                (*ret_info)["server_ips"] += ",";
                (*ret_info)["server_ports"] += ip_port.second;
                (*ret_info)["server_ports"] += ",";
        }
    }
    p_ret_msg->set_target_messenger_id(mid);
    std::vector<msg_ptr> v{p_ret_msg};
    Messenger::get()->send(v);
    return 0;
}

stamp_t Scheduler::handle_register_node(msg_ptr msg) {
    if (FLAGS_debug) {
        LOG(INFO) << "[REGISTER_NODE] register node req from "
            << msg->sender_messenger_id();
    }
    msnger_id_t mid = msg->sender_messenger_id();
    int nid = msg->sender_node_id();
    auto &step2mnids = job2step2mnid_set_[msg->job_name()];
    std::lock_guard<std::mutex> sync_lock(sync_mutex_);
    if (step2mnids.empty()) {
        step2mnids[0];
    }
    auto max_iter = step2mnids.rbegin();
    (*max_iter).second.insert(get_mnid(mid, nid));
    return 0;
}

void Scheduler::start(bool barrier) {
    LOG(INFO) << "scheduler starting";
    handle_funcs_.emplace("register_node", std::bind(&Scheduler::handle_register_node, this, std::placeholders::_1));
    handle_funcs_.emplace("register", std::bind(&Scheduler::handle_register, this, std::placeholders::_1));
    handle_funcs_.emplace("sync", std::bind(&Scheduler::handle_sync, this, std::placeholders::_1));
    handle_funcs_.emplace("batch_done", std::bind(&Scheduler::handle_batch_done, this, std::placeholders::_1));
    std::thread t(&Scheduler::node_thread, this);
    t.detach();
    Messenger::get()->add_node(this);
    LOG(INFO) << "scheduler started";
    if (barrier) {
        //TODO barrier();
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
}

} //namespace

