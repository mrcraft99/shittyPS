#include <node.h>

DEFINE_int32(k_node_threads, 1, "num of node processing threads");
DEFINE_bool(debug, false, "debug node");

namespace sps {

void Node::node_thread() {
    while (true) {
        if (FLAGS_debug) {
            LOG(INFO) << "waiting for msg";
        }
        std::unique_lock<std::mutex> wait_lock(recv_mutex_);
        recv_condvar_.wait(wait_lock, [this]{
            return !mailbox.empty();
        });
        if (FLAGS_debug) {
            LOG(INFO) << "receiving msg";
        }
        auto p_msg = std::move(mailbox.front());
        mailbox.pop_front();
        recv_mutex_.unlock();
        process_msg(p_msg);
    }
}

void Node::process_msg(msg_ptr msg) {
    if (handle_funcs_.find(msg->cmd()) != handle_funcs_.end()) {
        handle_funcs_.at(msg->cmd())(msg);
    } else {
        LOG(INFO) << "got " << msg->cmd() << " msg";
    }
}

} //namespace
