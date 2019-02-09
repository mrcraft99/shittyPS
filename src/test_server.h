#pragma once
#include "node.h"
#include "messenger.h"
#include "message.pb.h"

namespace sps {
using fid2vec = std::unordered_map<uint64_t, std::vector<double>>;

class Server : public Node {
public:
    Server(int id, bool barrier=true)
        : Node(id) { start(barrier); }
    void start(bool barrier) override;

private:
    stamp_t handle_pull(msg_ptr msg);
    stamp_t handle_push(msg_ptr msg);
    virtual void apply_pgm() {} //TODO Proximal Gradient Methods
    fid2vec memory_;
    std::mutex memory_mutex_;
};
}
