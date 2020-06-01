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
#include <nlohmann/json.hpp>

struct OutputFilter {
    std::string regex;
    std::string output;
    std::string var;
    std::regex compiled;
};

void replaceAll(std::string& out, const std::string& what, const std::string& by);

class GtpClient {
private:
    boost::process::opstream pis;
    boost::process::ipstream pes;
    boost::process::ipstream pos;
    boost::process::child c;
    std::string exe;
    std::string lastLine;
    std::vector<OutputFilter> outputFilters;
    InputThread<GtpClient, boost::process::ipstream> *reader;
    nlohmann::json vars;
public:
    typedef std::vector<std::string> CommandOutput;
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path): exe(exe), vars({}) {
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

    void interpolate(std::string& out) {
        for(auto it = vars.begin(); it != vars.end(); ++it) {
            replaceAll(out, it.key(), it.value());
        }
    }

    void compileFilters() {
        for (auto &re: outputFilters) {
            //prepare dynamic regex by replacing $vars
            std::string regex(re.regex);
            interpolate(regex);
            spdlog::trace("dynamic regex template [{}] compiled as [{}]...", re.regex, regex);
            try {
                re.compiled = std::regex(regex);
            }
            catch (const std::regex_error& e) {
                spdlog::error("malformed regular expression [{}]: {}", regex, e.what());
            }
        }
    }

    void operator()(const std::string& line) {
        //TODO
        spdlog::debug("gtp err = {}", line);
        for (auto &re: outputFilters) {
            std::smatch m;
            if(std::regex_search(line, m,  re.compiled)) {
                std::string output(re.output);
                for(size_t i = 0; i < m.size(); ++i) {
                    std::ostringstream ss;
                    ss << "$" << i;
                    replaceAll(output, ss.str(), m[i].str());
                }
                spdlog::trace("output template matched [{}]...", output);
                interpolate(output);
                if(!re.var.empty()) {
                    vars[re.var] = output;
                    compileFilters();
                } else {
                    lastLine = output;
                }
            }
        }
    }

    void addOutputFilter(const std::string& msg, const std::string& format, const std::string& var) {
        outputFilters.push_back({msg, format, var});
        compileFilters();
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

