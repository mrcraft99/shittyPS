#pragma once
#include <deque>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "util.h"
#include <glog/logging.h>
#include <gflags/gflags.h>

DECLARE_bool(debug);
namespace sps {

using handle_func = std::function<stamp_t(msg_ptr)>;

class Node {
public:
    virtual void start(bool barrier) = 0;
    virtual void stop() {}
    //TODO void barrier();
    virtual ~Node() {}
    Node (int id) : node_id_(id) {}
    int node_id() { return node_id_; }

    virtual void node_thread();
    virtual void process_msg(msg_ptr msg);

    std::deque<msg_ptr> mailbox;
    std::condition_variable recv_condvar_;
    std::mutex recv_mutex_;

protected:
    int node_id_; //TODO 0 for now
    std::unordered_map<stamp_t, call_back_func> callbacks_;
    std::unordered_map<std::string, handle_func> handle_funcs_;

};

} //namespace
