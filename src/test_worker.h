#pragma once
#include "node.h"
#include "messenger.h"

namespace sps {

class Worker : public Node {
public:
    Worker(std::string job_name, int id, bool barrier=false)
            : Node(id), job_name_(job_name) { start(barrier); }

    void start(bool barrier) override;
    void stop() override;

    void push(fid_vec_ptr fids, val_vec_ptr vals, const call_back_func &cb, bool sync=false);
    stamp_t pull(fid_vec_ptr fids, const call_back_func &cb);
    stamp_t sync(int delay, const call_back_func &cb, bool wait_sync=true);
    void wait(stamp_t stamp);

    void default_pull_cb(fid_vec_ptr fids, val_vec_ptr vals, const Message &msg);
    void default_push_cb(const Message &msg);
    void simple_slice_kv(fid_vec_ptr fids, std::map<int32_t, std::vector<size_t>> &mid2idx);
    std::string job_name() { return job_name_; }
    static std::vector<msnger_id_t> server_list;

private:
    void process_msg(msg_ptr msg) override;
    std::string job_name_;
    std::unordered_map<stamp_t, int> pending_count_;
    std::condition_variable wait_condvar_;
    std::mutex wait_mutex_;
    std::mutex write_mutex_;
};
}
