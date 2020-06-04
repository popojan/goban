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
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path);

    ~GtpClient();

    void interpolate(std::string& out);

    void operator()(const std::string& line);

    void compileFilters();

    void addOutputFilter(const std::string& msg, const std::string& format, const std::string& var);

    std::string lastError();

    CommandOutput showboard();

    CommandOutput name();

    CommandOutput version();

    CommandOutput issueCommand(const std::string& command);

    static bool success(const CommandOutput& ret);

};

#endif // GTPCLIENT_H

