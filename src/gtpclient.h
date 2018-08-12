#ifndef GTPCLIENT_H
#define GTPCLIENT_H

#ifdef  WIN_32
#pragma warning( disable : 4512 4702 4996)
#endif
#include <boost/process.hpp>
#include <boost/filesystem/path.hpp>
#undef __GNUC__
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#define __GNUC__ 1
#pragma GCC diagnostic pop
#ifdef  WIN_32
#pragma warning( default: 4512 4702 4996)
#endif

#include <string>
#include <vector>
#include <iostream>

class GtpClient {
private:
    boost::iostreams::stream<boost::iostreams::file_descriptor_sink> pis;
    boost::iostreams::stream<boost::iostreams::file_descriptor_source> pos;
public:
    typedef std::vector<std::string> CommandOutput;
    GtpClient(const std::string& exe, const std::string& cmdline, const std::string& path) {
        std::string gnugo = boost::process::search_path(exe, path);

        std::cerr << gnugo << std::endl;

        boost::filesystem::path file(gnugo);

        boost::process::pipe pout = boost::process::create_pipe();
        boost::process::pipe pin  = boost::process::create_pipe();

        boost::iostreams::file_descriptor_sink psink(pout.sink, boost::iostreams::file_descriptor_flags::close_handle);
        boost::iostreams::file_descriptor_source psource(pin.source, boost::iostreams::file_descriptor_flags::close_handle);

        boost::process::execute(
            boost::process::initializers::run_exe(file),
            boost::process::initializers::set_cmd_line(cmdline),
            boost::process::initializers::bind_stdout(psink),
            boost::process::initializers::bind_stdin(psource)
        );

        boost::iostreams::file_descriptor_source source(pout.source, boost::iostreams::file_descriptor_flags::close_handle);
        boost::iostreams::file_descriptor_sink sink(pin.sink, boost::iostreams::file_descriptor_flags::close_handle);

        pis.open(sink);
        pos.open(source);

    }
    ~GtpClient() {
        pis.close();
        pos.close();
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
        std::cerr << "sendline = [" << command << "]" << std::endl;
        pis << command << std::endl;
        pis.flush();
        std::string line;
        while (std::getline(pos, line)) {
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            std::cerr << "getline = [" << line << "]" << std::endl;
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

