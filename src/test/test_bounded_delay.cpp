#include <chrono>
#include <random>
#include <functional>
#include <thread>
#include <glog/logging.h>
#include "messenger.h"
#include "test_worker.h"
#include "test_scheduler.h"
#include "test_server.h"
#include "util.h"

using namespace sps;
const int NUM_FID = 10;
const size_t VEC_LEN = 8;
int k_worker_threads = 2;
std::vector<std::thread> threads;
DECLARE_string(role);

void run_server_sync() {
    auto p_server = std::make_shared<Server>(0);
    return;
}

void run_scheduler_sync() {
    auto p_scheduler = std::make_shared<Scheduler>(0);
    return;
}

void run_worker_thread(fid_vec_ptr fids, int slow, int id) {
    auto p_worker = std::make_shared<Worker>("tiktok_my_shitty_model", id);
    if (slow) {
        LOG(INFO) << id << " sleeping";
        std::this_thread::sleep_for(std::chrono::seconds(slow));
    }
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> val_dis(-1.0, 1.0);
    //random generated vectors
    std::cout << "vectors0 node: " << id << std::endl;
    auto vals = std::make_shared<val_vec>(NUM_FID);
    for (int i = 0; i < NUM_FID; ++i) {
        std::cout << fids->at(i) << ": [";
        for (int _ = 0; _ != VEC_LEN; ++_) {
            double value = val_dis(rng);
            (*vals)[i].push_back(value);
            std::cout << value << ", ";
        }
        std::cout << "]\n";
    }
    std::cout << '\n';
    p_worker->push(fids, vals, std::bind(&Worker::default_push_cb, p_worker, std::placeholders::_1), /*sync=*/true);
    if (slow) {
        LOG(INFO) << id << " sleeping";
        std::this_thread::sleep_for(std::chrono::seconds(slow));
    }

    auto ret_fids = std::make_shared<fid_vec>();
    vals->clear();
    p_worker->sync(0, [](Message &msg){}, true);
    auto stamp = p_worker->pull(fids, std::bind(&Worker::default_pull_cb, p_worker, ret_fids, vals, std::placeholders::_1));
    p_worker->wait(stamp);
    fids = ret_fids;
    std::cout << "vectors0 node: " << id << " pull" << std::endl;
    for (size_t i = 0; i < fids->size(); ++i) {
        std::cout << (*fids)[i] << ": [";
        for (auto val: (*vals)[i]) {
            std::cout << val << ", ";
        }
        std::cout << "]\n";
    }
    for (int i = 0; i < NUM_FID; ++i) {
        (*vals)[i].clear();
    }
    //random generate vectors
    std::cout << "vectors1 node: " << id << std::endl;
    for (size_t i = 0; i != fids->size(); ++i) {
        std::cout << (*fids)[i] << ": [";
        for (int _ = 0; _ < VEC_LEN; ++_) {
            double value = val_dis(rng);
            (*vals)[i].push_back(value);
            std::cout << value << ", ";
        }
        std::cout << "]\n";
    }
    std::cout << '\n';
    p_worker->push(fids, vals, std::bind(&Worker::default_push_cb, p_worker, std::placeholders::_1), /*sync=*/true);
    if (slow) {
        LOG(INFO) << id << " sleeping";
        std::this_thread::sleep_for(std::chrono::seconds(slow)); //for test
    }

    //pull
    vals->clear();
    ret_fids = std::make_shared<fid_vec>();
    p_worker->sync(0, [](Message &msg){}, true);
    stamp = p_worker->pull(fids, std::bind(&Worker::default_pull_cb, p_worker, ret_fids, vals, std::placeholders::_1));
    fids = ret_fids;
    p_worker->wait(stamp);
    std::cout << "vectors1 node: " << id << " pull" << std::endl;
    for (size_t i = 0; i < fids->size(); ++i) {
        std::cout << (*fids)[i] << ": [";
        for (auto val: (*vals)[i]) {
            std::cout << val << ", ";
        }
        std::cout << "]\n";
    }
}

void run_worker_sync() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint64_t> fid_dis(static_cast<uint64_t>(1)<<54, static_cast<uint64_t>(1023)<<54);
    auto fids = std::make_shared<fid_vec>();
    for (int i = 0; i < NUM_FID; ++i) {
        uint64_t fid = fid_dis(rng);
        fids->push_back(fid);
    }
    for (int i = 0; i < k_worker_threads ; ++i) {
        threads.emplace_back(run_worker_thread, fids, 5*i+2, i);
    }
    for (auto &t: threads) {
        t.join();
    }
}


int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "usage: test_async {role}" << std::endl;
        return 1;
    }
    std::string flagfile(*(argv+1));
    if (!init_flag(flagfile)) {
        std::cout << ".conf file to init flags not found" << std::endl;
        return 1;
    }
    init_logging(/*debug=*/true);
    LOG(INFO) << "test bounded delay";

    Messenger::get()->start();

    std::map<std::string, std::function<void(void)>> main_func_map {
                                            {"server", run_server_sync},
                                            {"worker", run_worker_sync},
                                            {"scheduler", run_scheduler_sync}};
    if (main_func_map.find(FLAGS_role) != main_func_map.end()) {
        main_func_map[FLAGS_role]();
        Messenger::get()->stop();
    } else {
        std::cout << "not a valid role" << std::endl;
    }
    return 0;
}
