#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#include <boost/process.hpp>
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <spdlog/spdlog.h>
#include "InputThread.h"
#include <regex>

class GtpClient {
private:
    boost::process::opstream pis;
    boost::process::ipstream pes;
    boost::process::ipstream pos;
    boost::process::child c;
    std::string exe;
    std::string lastLine;
    std::vector<std::regex> outputFilters;
    InputThread<GtpClient, boost::process::ipstream> *reader;
public:
    typedef std::vector<std::string> CommandOutput;
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path): exe(exe) {
        spdlog::info("Starting GTP client [{}/{}]", path, exe);

        auto where(::boost::this_process::path());
        where.push_back(path);
        auto gnugo = boost::process::search_path(exe, where);

        boost::filesystem::path file(gnugo);

        std::istringstream iss(cmdline);
        std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                         std::istream_iterator<std::string>());
        spdlog::info("running child");
        c = boost::process::child(file, params,
                                  boost::process::std_out > pos,
                                  boost::process::std_in < pis,
                                  boost::process::std_err > pes);
        reader = new InputThread<GtpClient, boost::process::ipstream>(pes);
        reader->bind(*this);
    }
    ~GtpClient() {
        delete reader;
    }

    void operator()(const std::string& line) {
        //TODO
        spdlog::debug("gtp err = {}", line);
        for (auto &re: outputFilters) {
            std::smatch m;
            if(std::regex_search(line, m,  re)){
                if(m.size() > 1) {
                    lastLine = m[1].str();
                }
                else if(m.size() > 0) {
                    lastLine = m[0].str();
                }
            }
        }
    }

    void addOutputFilter(const std::string& msg) {
        outputFilters.push_back(std::regex(msg));
    }

    std::string lastError() {
        return lastLine;
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
        spdlog::info("{1} << {0}", command, exe);
        pis << command << std::endl;
        std::string line;
        spdlog::info("getting response...");
        bool error = true;
        while (std::getline(pos, line)) {
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            if(ret.empty())
                error = *line.c_str() != '=';
            if(!error)
                spdlog::info("{1} >> {0}", line, exe);
            else
                spdlog::error("{1} >> {0}", line, exe);
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

