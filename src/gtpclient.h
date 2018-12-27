#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#include "spdlog/spdlog.h"
#include <boost/process.hpp>
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

class GtpClient {
private:
    boost::process::opstream pis;
    boost::process::ipstream pos;
    boost::process::child c;
    std::shared_ptr<spdlog::logger> console;
    std::string exe;
public:
    typedef std::vector<std::string> CommandOutput;
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path): exe(exe) {
        console = spdlog::get("console");
        console->info("Starting GTP client [{}/{}]", path, exe);

        auto where(::boost::this_process::path());
        where.push_back(path);
        auto gnugo = boost::process::search_path(exe, where);

        boost::filesystem::path file(gnugo);

        std::istringstream iss(cmdline);
        std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        console->info("running child");
        c = boost::process::child(file, params, boost::process::std_out > pos, boost::process::std_in < pis, boost::process::std_err.close());
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
        console->info("{1} << {0}", command, exe);
        pis << command << std::endl;
        std::string line;
        console->info("getting response...");
        while (std::getline(pos, line)) {
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            console->info("{1} >> {0}", line, exe);
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

