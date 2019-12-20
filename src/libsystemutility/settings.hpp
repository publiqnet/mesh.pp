#pragma once

#include "global.hpp"

#include <boost/filesystem.hpp>

#include <string>
#include <memory>

namespace meshpp
{
SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path config_directory_path();
SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path data_directory_path();

SYSTEMUTILITYSHARED_EXPORT void create_config_directory();
SYSTEMUTILITYSHARED_EXPORT void create_data_directory();

SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path config_file_path(std::string const& file);
SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path data_file_path(std::string const& file);
SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path data_directory_path(std::string const& dir);
SYSTEMUTILITYSHARED_EXPORT boost::filesystem::path data_directory_path(std::string const& dir1, std::string const& dir2);


class SYSTEMUTILITYSHARED_EXPORT settings
{
public:
    static void set_data_directory(std::string const& data_dir);
    static std::string data_directory();

    static void set_application_name(std::string const& application_name);
    static std::string application_name();
private:
    static std::string s_data_dir;
    static std::string s_application_name;
};
}// end of namespace meshpp
