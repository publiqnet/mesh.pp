#include "settings.hpp"

#include <mesh.pp/global.hpp>

#include <boost/filesystem.hpp>

#include <vector>
#include <exception>

#if defined(M_OS_LINUX)

#include <cstdlib>

#elif defined(M_OS_WINDOWS)

#include <shlobj.h>

#elif defined(M_OS_MACOS)

#include <sysdir.h>
#include <climits>

#else
#error config_dir function has not been implemented for your platform!
#endif

namespace filesystem = boost::filesystem;
using std::string;
using std::runtime_error;
using std::fstream;
using std::vector;

#if defined(M_OS_MACOS)

#include <glob.h>
#include <cassert>
#include <string>
#include <cstring>

void ExpandTildePath(char const* compact_path, char* expanded_path)
{
    glob_t globbuf;

    assert(compact_path && expanded_path);

    if (nullptr == compact_path ||
        nullptr == expanded_path)
        throw runtime_error("ExpandTildePath wring input");

    if (glob(compact_path, GLOB_TILDE, nullptr, &globbuf))
        throw runtime_error("ExpandTildePath glob(compact_path, GLOB_TILDE, nullptr, &globbuf): " + std::string(compact_path));

    if (1 != globbuf.gl_pathc)
        throw runtime_error("ExpandTildePath gl_pathc: " + std::to_string(globbuf.gl_pathc));

    strcpy(expanded_path, globbuf.gl_pathv[0]);

    globfree(&globbuf);
}


#endif

namespace meshpp
{
filesystem::path home_directory_path()
{
#ifdef M_OS_LINUX
    char* home = getenv("HOME");
    if (nullptr == home)
        throw runtime_error("HOME environment"
                            " variable is specified");

    filesystem::path fs_home(home);

#elif defined(M_OS_WINDOWS)
    wchar_t home[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, home)))
        throw runtime_error("SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, home)");

    filesystem::path fs_home(home);

#elif defined(M_OS_MACOS)
    char home[PATH_MAX];
    ExpandTildePath("~", home);

    filesystem::path fs_home(home);
#endif

    if (fs_home.empty() ||
        false == filesystem::is_directory(fs_home))
        throw runtime_error("detected invalid home directory: " + fs_home.string());

    return fs_home;
}
/*
 *   Windows: C:\Users\username\AppData\Roaming\appname\
 *   Linux: /home/username/.config/appname/
 *   Mac: /Users/username/Library/Application Support/appname/
 */
filesystem::path config_directory_path()
{
#ifdef M_OS_LINUX
    filesystem::path fs_config;

    char* config_home = getenv("XDG_CONFIG_HOME");

    if (nullptr != config_home)
    {
        fs_config = filesystem::path(config_home);
    }
    else
    {
        char* home = getenv("HOME");
        if (nullptr == home)
            throw runtime_error("neither XDG_CONFIG_HOME nor HOME environment"
                                " variable is specified");

        fs_config = filesystem::path(home) / ".config";
    }

    if (fs_config.empty() ||
        false == filesystem::is_directory(fs_config))
        throw runtime_error("detected invalid config directory: " + fs_config.string());

#elif defined(M_OS_WINDOWS)
    wchar_t appdata[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata)))
        throw runtime_error("SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata)");

    filesystem::path fs_config(appdata);

#elif defined(M_OS_MACOS)
    char config[PATH_MAX];
    sysdir_search_path_enumeration_state state = sysdir_start_search_path_enumeration(SYSDIR_DIRECTORY_APPLICATION_SUPPORT, SYSDIR_DOMAIN_MASK_USER);
    if ((state = sysdir_get_next_search_path_enumeration(state, config)) == 0)
        throw runtime_error("(state = sysdir_get_next_search_path_enumeration(state, path)) == 0");

    ExpandTildePath(config, config);

    filesystem::path fs_config(config);
#endif

    fs_config /= settings::application_name();

    return fs_config;
}

filesystem::path data_directory_path()
{
    filesystem::path fs_data(settings::data_directory());
#if BOOST_VERSION >= 106000
    return filesystem::weakly_canonical(fs_data);
#else
    return fs_data;
#endif
}

void create_config_directory()
{
    filesystem::create_directory(config_directory_path());
}
void create_data_directory()
{
    filesystem::create_directory(data_directory_path());
}

void reset_data_directory()
{
    filesystem::remove_all(data_directory_path());
    filesystem::create_directory(data_directory_path());
}

enum class config_data {config, data};
filesystem::path file_path(vector<string> const& dirs, string const& file, config_data cd)
{
    filesystem::path fs_path = config_directory_path();
    if (cd == config_data::data)
        fs_path = data_directory_path();

    for (auto const& dir : dirs)
    {
        if (dir.empty())
            continue;

        fs_path /= dir;
        if (false == filesystem::exists(fs_path))
            filesystem::create_directory(fs_path);
    }

    if (false == file.empty())
        fs_path /= file;

    return fs_path;
}

filesystem::path config_file_path(string const& file)
{
    return file_path(vector<string>(), file, config_data::config);
}

filesystem::path data_file_path(string const& file)
{
    return file_path(vector<string>(), file, config_data::data);
}
filesystem::path data_directory_path(string const& dir)
{
    return file_path({dir}, string(), config_data::data);
}
filesystem::path data_directory_path(string const& dir1, string const& dir2)
{
    return file_path({dir1, dir2}, string(), config_data::data);
}

string settings::s_application_name = "mesh_application";
string settings::s_data_dir = config_directory_path().string();

void settings::set_data_directory(string const& data_dir)
{
    s_data_dir = data_dir;
    if (s_data_dir.find("~") == 0)
        s_data_dir.replace(0, 1, home_directory_path().string());
}

string settings::data_directory()
{
    return s_data_dir;
}

void settings::set_application_name(string const& application_name)
{
    s_application_name = application_name;
}

string settings::application_name()
{
    return s_application_name;
}
}// end namespace meshpp
