#include <boost/filesystem.hpp>
#include <util.h>
#include <glog/logging.h>
#include <gflags/gflags.h>

DEFINE_string(log_file, "log.log", "log file name");
DEFINE_string(log_path, "./", "log file path");

namespace sps {

bool init_flag(const std::string &flag_file) {
    // init gflag
    int argc = 3;
    std::string main = "main";
    std::string flag = "--flagfile";
    boost::filesystem::path path(flag_file);
    boost::system::error_code error;
    if (!boost::filesystem::is_regular_file(path, error)) {
        return false;
    }
    // boost::filesystem::path filepath = boost::filesystem::system_complete(path);
    //char* test = new char[path.string().length() + 1];
    //strcpy(test, path.string().c_str());
    char* argv[3];
    argv[0] = const_cast<char*>(main.c_str());
    argv[1] = const_cast<char*>(flag.c_str());
    argv[2] = const_cast<char*>(path.string().c_str());
    char **argvp = argv;
    gflags::ParseCommandLineFlags(&argc, &argvp, true);
    return true;
}

void init_logging(bool is_debug) {
    if (is_debug) {
        google::SetStderrLogging(google::GLOG_INFO);
    } else {
        google::SetLogDestination(google::GLOG_INFO, FLAGS_log_path.c_str());
        google::InitGoogleLogging(FLAGS_log_file.c_str());
    }
}

} //namespace sps
