#pragma once
#include <boost/asio.hpp>
#include <memory>
#include "message.pb.h"

//TODO Context class to contain them
namespace sps {
using namespace boost::asio;
using msg_ptr = std::shared_ptr<Message>;
using stamp_t = uint64_t;
using msnger_id_t = int32_t;
using fid_vec = std::vector<uint64_t>;
using fid_vec_ptr = std::shared_ptr<std::vector<uint64_t>>;
using val_vec = std::vector<std::vector<double>>;
using val_vec_ptr = std::shared_ptr<std::vector<std::vector<double>>>;
using socket_ptr = std::shared_ptr<ip::tcp::socket>;
using call_back_func = std::function<void(Message&)>;

const uint64_t MESSAGE_MAX_SIZE = 1024;
const msnger_id_t SCHEDULER = 0;

bool init_flag(const std::string &role_str);
void init_logging(bool is_debug);

} //namespace sps
