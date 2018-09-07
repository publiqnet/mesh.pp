#pragma once

#include "global.hpp"

#include <belt.pp/ilog.hpp>

#include <boost/filesystem/path.hpp>

#include <string>

namespace meshpp
{
MLOGSHARED_EXPORT beltpp::ilog_ptr file_logger(std::string const& name, boost::filesystem::path const& fname);
}
