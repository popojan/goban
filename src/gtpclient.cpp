#include "gtpclient.h"
#include <string>

GtpClient::GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path)
 : exe(exe), vars({})
{
    spdlog::info("Starting GTP client [{}/{}]", path, exe);

    std::vector<boost::filesystem::path> where(::boost::this_process::path());
    where.push_back(path);

    boost::filesystem::path file(boost::process::search_path(exe, where));
    spdlog::info("About to run GTP engine [{}]", file.generic_path().string());

    std::istringstream iss(cmdline);
    std::vector<std::string> params((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());

    spdlog::info("running child [{} {}]", file.generic_path().string(), cmdline);
    c = boost::process::child(file, params,
                              boost::process::std_out > pos,
                              boost::process::std_in < pis,
                              boost::process::std_err > pes);

    reader = new InputThread<GtpClient, boost::process::ipstream>(pes);
    reader->bind(*this);
}

void replaceAll(std::string& out, const std::string& what, const std::string& by) {
    size_t index(0);
    while (true) {
        spdlog::trace("replace [{}] by [{}] in [{}]", what, by, out);
        index = out.find(what, index);
        if (index == std::string::npos) break;
        out.replace(index, what.size(), by);
        index += what.size();
    }
}

void GtpClient::operator()(const std::string& line) {
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

void GtpClient::compileFilters() {
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

void GtpClient::addOutputFilter(const std::string& msg, const std::string& format, const std::string& var) {
    outputFilters.push_back({msg, format, var});
    compileFilters();
}

std::string GtpClient::lastError() {
    return lastLine;
}

GtpClient::CommandOutput GtpClient::showboard() {
    return issueCommand("showboard");
}
GtpClient::CommandOutput GtpClient::name() {
    return issueCommand("name");
}
GtpClient::CommandOutput GtpClient::version() {
    return issueCommand("version");
}
GtpClient::CommandOutput GtpClient::issueCommand(const std::string& command) {
    spdlog::info("{1} << {0}", command, exe);
    CommandOutput ret;
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

bool GtpClient::success(const CommandOutput& ret) {
    return ret.size() > 0 && ret.at(0).front() == '=';
};

GtpClient::~GtpClient() {
    c.wait();
    delete reader;
}

void GtpClient::interpolate(std::string& out) {
    for(auto it = vars.begin(); it != vars.end(); ++it) {
        replaceAll(out, it.key(), it.value());
    }
}