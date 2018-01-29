class message_ip_destination;
namespace detail
{
std::string saver(message_ip_destination const& self);
}
class message_ip_destination
{
public:
    enum {rtt = 0};
    int port;
    string address;
    message_ip_destination()
      : port()
      , address()
    {}
    template <typename T>
    explicit message_ip_destination(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_ip_destination& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_ip_destination* pmc = static_cast<message_ip_destination*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_ip_destination& self, T const& other)
{
    assign(self.port, other.port);
    assign(self.address, other.address);
}
template <typename T>
void assign(T& self, message_ip_destination const& other)
{
    assign(self.port, other.port);
    assign(self.address, other.address);
}
namespace detail
{
bool analyze_json(message_ip_destination& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_ip_destination::rtt)
        code = false;
    else
    {
        if (code)
        {
            auto it_find = members.find("\"port\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.port, item);
            }
        }
        if (code)
        {
            auto it_find = members.find("\"address\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.address, item);
            }
        }
    }
    return code;
}
std::string saver(message_ip_destination const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_ip_destination::rtt);
    result += ",\"port\" : " + saver(self.port);
    result += ",\"address\" : " + saver(self.address);
    result += "}";
    return result;
}
}


class message_ip_address;
namespace detail
{
std::string saver(message_ip_address const& self);
}
class message_ip_address
{
public:
    enum {rtt = 1};
    int type;
    message_ip_destination local;
    message_ip_destination remote;
    message_ip_address()
      : type()
      , local()
      , remote()
    {}
    template <typename T>
    explicit message_ip_address(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_ip_address& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_ip_address* pmc = static_cast<message_ip_address*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_ip_address& self, T const& other)
{
    assign(self.type, other.type);
    assign(self.local, other.local);
    assign(self.remote, other.remote);
}
template <typename T>
void assign(T& self, message_ip_address const& other)
{
    assign(self.type, other.type);
    assign(self.local, other.local);
    assign(self.remote, other.remote);
}
namespace detail
{
bool analyze_json(message_ip_address& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_ip_address::rtt)
        code = false;
    else
    {
        if (code)
        {
            auto it_find = members.find("\"type\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.type, item);
            }
        }
        if (code)
        {
            auto it_find = members.find("\"local\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.local, item);
            }
        }
        if (code)
        {
            auto it_find = members.find("\"remote\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.remote, item);
            }
        }
    }
    return code;
}
std::string saver(message_ip_address const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_ip_address::rtt);
    result += ",\"type\" : " + saver(self.type);
    result += ",\"local\" : " + saver(self.local);
    result += ",\"remote\" : " + saver(self.remote);
    result += "}";
    return result;
}
}


class message_join;
namespace detail
{
std::string saver(message_join const& self);
}
class message_join
{
public:
    enum {rtt = 2};
    static std::vector<char> saver(void* p)
    {
        message_join* pmc = static_cast<message_join*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_join& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_join::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_join const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_join::rtt);
    result += "}";
    return result;
}
}


class message_drop;
namespace detail
{
std::string saver(message_drop const& self);
}
class message_drop
{
public:
    enum {rtt = 3};
    static std::vector<char> saver(void* p)
    {
        message_drop* pmc = static_cast<message_drop*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_drop& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_drop::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_drop const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_drop::rtt);
    result += "}";
    return result;
}
}


class message_ping;
namespace detail
{
std::string saver(message_ping const& self);
}
class message_ping
{
public:
    enum {rtt = 4};
    string nodeid;
    message_ping()
      : nodeid()
    {}
    template <typename T>
    explicit message_ping(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_ping& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_ping* pmc = static_cast<message_ping*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_ping& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
}
template <typename T>
void assign(T& self, message_ping const& other)
{
    assign(self.nodeid, other.nodeid);
}
namespace detail
{
bool analyze_json(message_ping& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_ping::rtt)
        code = false;
    else
    {
        if (code)
        {
            auto it_find = members.find("\"nodeid\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.nodeid, item);
            }
        }
    }
    return code;
}
std::string saver(message_ping const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_ping::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
    result += "}";
    return result;
}
}


class message_pong;
namespace detail
{
std::string saver(message_pong const& self);
}
class message_pong
{
public:
    enum {rtt = 5};
    string nodeid;
    message_pong()
      : nodeid()
    {}
    template <typename T>
    explicit message_pong(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_pong& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_pong* pmc = static_cast<message_pong*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_pong& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
}
template <typename T>
void assign(T& self, message_pong const& other)
{
    assign(self.nodeid, other.nodeid);
}
namespace detail
{
bool analyze_json(message_pong& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_pong::rtt)
        code = false;
    else
    {
        if (code)
        {
            auto it_find = members.find("\"nodeid\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.nodeid, item);
            }
        }
    }
    return code;
}
std::string saver(message_pong const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_pong::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
    result += "}";
    return result;
}
}


class message_error;
namespace detail
{
std::string saver(message_error const& self);
}
class message_error
{
public:
    enum {rtt = 6};
    static std::vector<char> saver(void* p)
    {
        message_error* pmc = static_cast<message_error*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_error& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_error::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_error const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_error::rtt);
    result += "}";
    return result;
}
}


class message_time_out;
namespace detail
{
std::string saver(message_time_out const& self);
}
class message_time_out
{
public:
    enum {rtt = 7};
    static std::vector<char> saver(void* p)
    {
        message_time_out* pmc = static_cast<message_time_out*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_time_out& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_time_out::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_time_out const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_time_out::rtt);
    result += "}";
    return result;
}
}


class message_get_peers;
namespace detail
{
std::string saver(message_get_peers const& self);
}
class message_get_peers
{
public:
    enum {rtt = 8};
    static std::vector<char> saver(void* p)
    {
        message_get_peers* pmc = static_cast<message_get_peers*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_get_peers& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_get_peers::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_get_peers const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_get_peers::rtt);
    result += "}";
    return result;
}
}


class message_peer_info;
namespace detail
{
std::string saver(message_peer_info const& self);
}
class message_peer_info
{
public:
    enum {rtt = 9};
    message_ip_address address;
    message_peer_info()
      : address()
    {}
    template <typename T>
    explicit message_peer_info(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_peer_info& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_peer_info* pmc = static_cast<message_peer_info*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_peer_info& self, T const& other)
{
    assign(self.address, other.address);
}
template <typename T>
void assign(T& self, message_peer_info const& other)
{
    assign(self.address, other.address);
}
namespace detail
{
bool analyze_json(message_peer_info& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_peer_info::rtt)
        code = false;
    else
    {
        if (code)
        {
            auto it_find = members.find("\"address\"");
            if (it_find != members.end())
            {
                beltpp::json::expression_tree* item = it_find->second;
                assert(item);
                code = analyze_json(msgcode.address, item);
            }
        }
    }
    return code;
}
std::string saver(message_peer_info const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_peer_info::rtt);
    result += ",\"address\" : " + saver(self.address);
    result += "}";
    return result;
}
}


namespace detail
{
bool analyze_json_object(beltpp::json::expression_tree* pexp,
                         beltpp::detail::pmsg_all& return_value)
{
    bool code = false;
    switch (return_value.rtt)
    {
    case message_ip_destination::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_ip_destination::rtt,
                                            make_void_unique_ptr<message_ip_destination>(),
                                            &message_ip_destination::saver);
        message_ip_destination* pmsgcode =
                static_cast<message_ip_destination*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_ip_address::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_ip_address::rtt,
                                            make_void_unique_ptr<message_ip_address>(),
                                            &message_ip_address::saver);
        message_ip_address* pmsgcode =
                static_cast<message_ip_address*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_join::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_join::rtt,
                                            make_void_unique_ptr<message_join>(),
                                            &message_join::saver);
        message_join* pmsgcode =
                static_cast<message_join*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_drop::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_drop::rtt,
                                            make_void_unique_ptr<message_drop>(),
                                            &message_drop::saver);
        message_drop* pmsgcode =
                static_cast<message_drop*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_ping::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_ping::rtt,
                                            make_void_unique_ptr<message_ping>(),
                                            &message_ping::saver);
        message_ping* pmsgcode =
                static_cast<message_ping*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_pong::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_pong::rtt,
                                            make_void_unique_ptr<message_pong>(),
                                            &message_pong::saver);
        message_pong* pmsgcode =
                static_cast<message_pong*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_error::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_error::rtt,
                                            make_void_unique_ptr<message_error>(),
                                            &message_error::saver);
        message_error* pmsgcode =
                static_cast<message_error*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_time_out::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_time_out::rtt,
                                            make_void_unique_ptr<message_time_out>(),
                                            &message_time_out::saver);
        message_time_out* pmsgcode =
                static_cast<message_time_out*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_get_peers::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_get_peers::rtt,
                                            make_void_unique_ptr<message_get_peers>(),
                                            &message_get_peers::saver);
        message_get_peers* pmsgcode =
                static_cast<message_get_peers*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_peer_info::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_peer_info::rtt,
                                            make_void_unique_ptr<message_peer_info>(),
                                            &message_peer_info::saver);
        message_peer_info* pmsgcode =
                static_cast<message_peer_info*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    default:
        code = false;
        break;
    }

    return code;
}
}
