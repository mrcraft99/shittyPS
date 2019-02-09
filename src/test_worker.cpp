#include <memory>
#include <glog/logging.h>
#include "test_worker.h"
#include <boost/lexical_cast.hpp>

namespace sps {

std::vector<msnger_id_t> Worker::server_list;

void Worker::push(fid_vec_ptr fids, val_vec_ptr vals, const call_back_func &cb, bool sync) {
    if (fids->size() != vals->size()) {
        LOG(ERROR) << "fids and vals size not match"
            << "size of fids: " << fids->size()
            << "size of vals: " << vals->size();
        return;
    }
    std::map<int32_t, std::vector<size_t>> mid2idx;
    simple_slice_kv(fids, mid2idx);
    std::vector<msg_ptr> v;
    for (auto iter = mid2idx.begin(); iter != mid2idx.end(); ++iter) {
        auto p_push_msg = std::make_shared<Message>();
        p_push_msg->set_cmd("push");
        p_push_msg->set_target_messenger_id(iter->first);
        //TODO
        //p_push_msg->set_job_name(job_name_);
        auto &push_idxs = iter->second;
        p_push_msg->mutable_fids()->Reserve(push_idxs.size());
        for (auto i: push_idxs) {
            p_push_msg->mutable_fids()->Add((*fids)[i]);
            auto p_vec = p_push_msg->add_vecs();
            p_vec->set_size((*vals)[i].size());
            p_vec->mutable_values()->Reserve((*vals)[i].size());
            for (auto val: (*vals)[i]) {
                p_vec->mutable_values()->Add(val);
            }
        }
        v.push_back(std::move(p_push_msg));
    }
    if (sync) {
        LOG(INFO) << "sending batch done msg";
        auto p_batch_msg = std::make_shared<Message>();
        p_batch_msg->set_cmd("batch_done");
        p_batch_msg->set_job_name(job_name_);
        p_batch_msg->set_target_messenger_id(SCHEDULER);
        p_batch_msg->set_sender_node_id(node_id());
        v.push_back(std::move(p_batch_msg));
    }
    Messenger::get()->send(v);
}

stamp_t Worker::pull(fid_vec_ptr fids, const call_back_func &cb) {
    stamp_t stamp = Messenger::get()->gen_stamp();
    stamp <<= 7;
    stamp += node_id();
    std::map<int32_t, std::vector<size_t>> mid2idx;
    simple_slice_kv(fids, mid2idx);
    std::vector<msg_ptr> v;
    for (auto iter = mid2idx.begin(); iter != mid2idx.end(); ++iter) {
        auto &pull_idxs = iter->second;
        auto p_pull_msg = std::make_shared<Message>();
        p_pull_msg->set_timestamp(stamp);
        p_pull_msg->set_cmd("pull");
        p_pull_msg->set_target_messenger_id(iter->first);
        p_pull_msg->set_sender_node_id(node_id());
        //TODO 0?
        p_pull_msg->mutable_fids()->Reserve(fids->size());
        for (auto i: pull_idxs) {
            p_pull_msg->mutable_fids()->Add((*fids)[i]);
        }
        v.push_back(std::move(p_pull_msg));
        std::lock_guard<std::mutex> pending_lock(wait_mutex_);
        callbacks_[stamp] = std::move(cb);
        pending_count_[stamp] += 1;
        LOG(INFO) << "pull pending";
    }
    Messenger::get()->send(v);
    return stamp;
}

stamp_t Worker::sync(int delay, const call_back_func &cb, bool wait_sync) {
    // delay n:bounded delay; 0:sequential; -1:eventual
    auto p_sync_msg = std::make_shared<Message>();
    auto req_info = p_sync_msg->mutable_cmd_info();
    std::vector<msg_ptr> v;
    p_sync_msg->set_cmd("sync");
    p_sync_msg->set_target_messenger_id(SCHEDULER);
    p_sync_msg->set_sender_node_id(node_id());
    (*req_info)["delay"] = boost::lexical_cast<std::string>(delay);
    stamp_t stamp = Messenger::get()->gen_stamp();
    stamp <<= 7;
    stamp += node_id();
    p_sync_msg->set_timestamp(stamp);
    p_sync_msg->set_job_name(job_name());
    v.push_back(p_sync_msg);
    {
        std::lock_guard<std::mutex> pending_lock(wait_mutex_);
        callbacks_[stamp] = std::move(cb);
        pending_count_[stamp] = 1;
    }
    if (FLAGS_debug) {
        LOG(INFO) << stamp << " sync pending";
    }
    Messenger::get()->send(v);
    if (wait_sync) {
        wait(stamp);
    }
    return stamp;
}

void Worker::default_push_cb(const Message &msg) {
    return;
}

void Worker::default_pull_cb(fid_vec_ptr fids, val_vec_ptr vals, const Message &msg) {
    {
        std::lock_guard<std::mutex> write_lock(write_mutex_);
        for (auto fid: msg.fids()) {
            fids->push_back(fid);
        }
        for (auto &vec: msg.vecs()) {
            std::vector<double> val;
            if (vec.size() > 0) {
                for (auto v: vec.values()) {
                    val.push_back(v);
                }
            }
            vals->push_back(std::move(val));
        }
    }
    LOG(INFO) << "end cb, ";
}

void Worker::simple_slice_kv(fid_vec_ptr fids, std::map<int32_t, std::vector<size_t>> &m) {
    for (size_t idx = 0; idx != fids->size(); ++idx) {
        auto n = (*fids)[idx] % server_list.size();
        m[server_list[n]].push_back(idx);
    }
}

void Worker::wait(stamp_t stamp) {
    std::unique_lock<std::mutex> wait_lock(wait_mutex_);
    wait_condvar_.wait(wait_lock, [stamp, this]{
        return (pending_count_[stamp] == 0);
    });
    pending_count_.erase(stamp);
    callbacks_.erase(stamp);
}

void Worker::process_msg(msg_ptr msg) {
    //TODO
    if (msg->reply_timestamp()) {
        std::lock_guard<std::mutex> pending_lock(wait_mutex_);
        auto ts = msg->reply_timestamp();
        if (callbacks_.find(ts) != callbacks_.end()) {
            callbacks_[ts](*msg);
        }
        pending_count_[ts] -= 1;
        if (FLAGS_debug) {
            LOG(INFO) << "got reply for stamp: "
                << msg->reply_timestamp()
                << " pending num remain: "
                << pending_count_[ts];
        }
        wait_condvar_.notify_one();
    } else {
        LOG(INFO) << "???";
        LOG(INFO) << msg->cmd();
    }
}

void Worker::start(bool barrier) {
    LOG(INFO) << "worker starting";
    std::thread t(&Worker::node_thread, this);
    t.detach();
    Messenger::get()->add_node(this);
    LOG(INFO) << "worker started";
}

void Worker::stop() {
    //TODO
}
} //namespace sps
