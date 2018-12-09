#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#include <boost/process.hpp>
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "spdlog/spdlog.h"

class GtpClient {
private:
    boost::process::opstream pis;
    boost::process::ipstream pos;
    boost::process::child c;
    std::shared_ptr<spdlog::logger> console;
public:
    typedef std::vector<std::string> CommandOutput;
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path) {
        console = spdlog::get("console");
        console->info(exe);
        console->info(cmdline);

        auto gnugo = boost::process::search_path(exe, {path});

        std::cerr << gnugo << std::endl;

        boost::filesystem::path file(gnugo);

        std::istringstream iss(cmdline);
        std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        c = boost::process::child(file, params, boost::process::std_out > pos, boost::process::std_in < pis, boost::process::std_err.close());
        console->info("running child");
    }
    ~GtpClient() {

    }

    CommandOutput showboard() {
        return issueCommand("showboard");
    }
    CommandOutput name() {
        return issueCommand("name");
    }
    CommandOutput version() {
        return issueCommand("version");
    }
    CommandOutput issueCommand(const std::string& command) {
        CommandOutput ret;
        console->info("sendline = [{0}]", command);
        pis << command << std::endl;
        std::string line;
        console->info("get line");
        while (std::getline(pos, line)) {
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            console->info("getline = [{0}]", line);
            if (line.empty()) break;
            ret.push_back(line);
        }
        return ret;
    }
    static bool success(const CommandOutput& ret) {
        return ret.size() > 0 && ret.at(0).front() == '=';
    }

};

#endif // GTPCLIENT_H

