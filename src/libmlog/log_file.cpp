#include "log.hpp"

#include <belt.pp/utility.hpp>

#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <chrono>

namespace filesystem = boost::filesystem;

using std::cout;
using std::cerr;
using std::endl;
using std::string;

namespace chrono = std::chrono;
using chrono::system_clock;

namespace meshpp
{
class log_file : public beltpp::ilog
{
public:
    log_file(string const& name, filesystem::path const& file_name)
        : enabled_(true)
        , str_name(name)
        , file_name(file_name)
    {
        filesystem::ofstream of;
        of.open(file_name, std::ios_base::app);
        if (!of)
            throw std::runtime_error("cannot open file: " + file_name.string());
    }
    ~log_file() override {}

    std::string name() const noexcept override
    {
        return str_name;
    }
    bool enabled() const noexcept override
    {
        return enabled_;
    }
    void enable() noexcept override
    {
        enabled_ = true;
    }
    void disable() noexcept override
    {
        enabled_ = false;
    }

    void message(std::string const& value) override
    {
        if (false == enabled_)
            return;

        filesystem::ofstream of;
        of.open(file_name, std::ios_base::app);
        if (!of)
            return;

        std::time_t time_t_now = system_clock::to_time_t(system_clock::now());
        of << beltpp::gm_time_t_to_lc_string(time_t_now) << " - ";
        of << value << endl;
    }
    void warning(std::string const& value) override
    {
        if (false == enabled_)
            return;

        filesystem::ofstream of;
        of.open(file_name, std::ios_base::app);
        if (!of)
            return;

        std::time_t time_t_now = system_clock::to_time_t(system_clock::now());
        of << beltpp::gm_time_t_to_lc_string(time_t_now) << " - ";
        of << "Warning: " << value << endl;
    }
    void error(std::string const& value) override
    {
        if (false == enabled_)
            return;

        filesystem::ofstream of;
        of.open(file_name, std::ios_base::app);
        if (!of)
            return;

        std::time_t time_t_now = system_clock::to_time_t(system_clock::now());
        of << beltpp::gm_time_t_to_lc_string(time_t_now) << " - ";
        of << "Error:   " << value << endl;
    }

    bool enabled_;
    string str_name;
    filesystem::path file_name;
};

beltpp::ilog_ptr file_logger(string const& name, filesystem::path const& fname)
{
    log_file* p = new log_file(name, fname);
    return beltpp::ilog_ptr(p);
}
}
