#pragma once
#include "node.h"
#include "messenger.h"
#include "message.pb.h"

namespace sps {
class Scheduler : public Node {
public:
    Scheduler(int id, bool barrier=true) : Node(id) { start(barrier); }
    void start(bool barrier) override;

private:
    stamp_t handle_register(msg_ptr msg);
    stamp_t handle_register_node(msg_ptr msg);
    stamp_t handle_batch_done(msg_ptr msg);
    stamp_t handle_sync(msg_ptr msg);

    int get_mnid(msnger_id_t mid, int nid) {
        return mid * 100 + nid;
    }
    std::vector<int> server_messenger_ids_;
    std::vector<int> worker_messenger_ids_;
    std::unordered_map<int, std::pair<std::string, std::string>> messenger_id2ip_port_;
    std::unordered_map<std::string, std::map<int, std::set<int>>> job2step2mnid_set_;

    std::vector<msg_ptr> ready_msgs_;
    std::mutex sync_mutex_;
};
}
