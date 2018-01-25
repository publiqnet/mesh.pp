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


class message_code_join;
namespace detail
{
std::string saver(message_code_join const& self);
}
class message_code_join
{
public:
    enum {rtt = 2};
    static std::vector<char> saver(void* p)
    {
        message_code_join* pmc = static_cast<message_code_join*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_code_join& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_join::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_code_join const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_join::rtt);
    result += "}";
    return result;
}
}


class message_code_drop;
namespace detail
{
std::string saver(message_code_drop const& self);
}
class message_code_drop
{
public:
    enum {rtt = 3};
    static std::vector<char> saver(void* p)
    {
        message_code_drop* pmc = static_cast<message_code_drop*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_code_drop& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_drop::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_code_drop const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_drop::rtt);
    result += "}";
    return result;
}
}


class message_code_ping;
namespace detail
{
std::string saver(message_code_ping const& self);
}
class message_code_ping
{
public:
    enum {rtt = 4};
    string nodeid;
    message_code_ping()
      : nodeid()
    {}
    template <typename T>
    explicit message_code_ping(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_code_ping& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_code_ping* pmc = static_cast<message_code_ping*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_code_ping& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
}
template <typename T>
void assign(T& self, message_code_ping const& other)
{
    assign(self.nodeid, other.nodeid);
}
namespace detail
{
bool analyze_json(message_code_ping& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_ping::rtt)
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
std::string saver(message_code_ping const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_ping::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
    result += "}";
    return result;
}
}


class message_code_pong;
namespace detail
{
std::string saver(message_code_pong const& self);
}
class message_code_pong
{
public:
    enum {rtt = 5};
    string nodeid;
    message_code_pong()
      : nodeid()
    {}
    template <typename T>
    explicit message_code_pong(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_code_pong& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_code_pong* pmc = static_cast<message_code_pong*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_code_pong& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
}
template <typename T>
void assign(T& self, message_code_pong const& other)
{
    assign(self.nodeid, other.nodeid);
}
namespace detail
{
bool analyze_json(message_code_pong& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_pong::rtt)
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
std::string saver(message_code_pong const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_pong::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
    result += "}";
    return result;
}
}


class message_code_error;
namespace detail
{
std::string saver(message_code_error const& self);
}
class message_code_error
{
public:
    enum {rtt = 6};
    static std::vector<char> saver(void* p)
    {
        message_code_error* pmc = static_cast<message_code_error*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_code_error& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_error::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_code_error const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_error::rtt);
    result += "}";
    return result;
}
}


class message_code_time_out;
namespace detail
{
std::string saver(message_code_time_out const& self);
}
class message_code_time_out
{
public:
    enum {rtt = 7};
    static std::vector<char> saver(void* p)
    {
        message_code_time_out* pmc = static_cast<message_code_time_out*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
namespace detail
{
bool analyze_json(message_code_time_out& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_time_out::rtt)
        code = false;
    else
    {
    }
    return code;
}
std::string saver(message_code_time_out const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_time_out::rtt);
    result += "}";
    return result;
}
}


class message_code_get_peers;
namespace detail
{
std::string saver(message_code_get_peers const& self);
}
class message_code_get_peers
{
public:
    enum {rtt = 8};
    string nodeid;
    message_code_get_peers()
      : nodeid()
    {}
    template <typename T>
    explicit message_code_get_peers(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_code_get_peers& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_code_get_peers* pmc = static_cast<message_code_get_peers*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_code_get_peers& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
}
template <typename T>
void assign(T& self, message_code_get_peers const& other)
{
    assign(self.nodeid, other.nodeid);
}
namespace detail
{
bool analyze_json(message_code_get_peers& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_get_peers::rtt)
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
std::string saver(message_code_get_peers const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_get_peers::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
    result += "}";
    return result;
}
}


class message_code_peer_info;
namespace detail
{
std::string saver(message_code_peer_info const& self);
}
class message_code_peer_info
{
public:
    enum {rtt = 9};
    string nodeid;
    message_ip_address address;
    message_code_peer_info()
      : nodeid()
      , address()
    {}
    template <typename T>
    explicit message_code_peer_info(T const& other)
    {
        assign(*this, other);
    }
    template <typename T>
    message_code_peer_info& operator = (T const& other)
    {
        assign(*this, other);
        return *this;
    }
    static std::vector<char> saver(void* p)
    {
        message_code_peer_info* pmc = static_cast<message_code_peer_info*>(p);
        std::vector<char> result;
        std::string str_value = detail::saver(*pmc);
        std::copy(str_value.begin(), str_value.end(), std::back_inserter(result));
        return result;
    }
};
template <typename T>
void assign(message_code_peer_info& self, T const& other)
{
    assign(self.nodeid, other.nodeid);
    assign(self.address, other.address);
}
template <typename T>
void assign(T& self, message_code_peer_info const& other)
{
    assign(self.nodeid, other.nodeid);
    assign(self.address, other.address);
}
namespace detail
{
bool analyze_json(message_code_peer_info& msgcode,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    std::unordered_map<std::string, beltpp::json::expression_tree*> members;
    size_t rtt = -1;
    if (false == analyze_json_common(rtt, pexp, members) ||
        rtt != message_code_peer_info::rtt)
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
std::string saver(message_code_peer_info const& self)
{
    std::string result;
    result += "{";
    result += "\"rtt\" : " + saver(message_code_peer_info::rtt);
    result += ",\"nodeid\" : " + saver(self.nodeid);
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
                                            message_code_creator<message_ip_destination>(),
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
                                            message_code_creator<message_ip_address>(),
                                            &message_ip_address::saver);
        message_ip_address* pmsgcode =
                static_cast<message_ip_address*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_join::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_join::rtt,
                                            message_code_creator<message_code_join>(),
                                            &message_code_join::saver);
        message_code_join* pmsgcode =
                static_cast<message_code_join*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_drop::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_drop::rtt,
                                            message_code_creator<message_code_drop>(),
                                            &message_code_drop::saver);
        message_code_drop* pmsgcode =
                static_cast<message_code_drop*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_ping::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_ping::rtt,
                                            message_code_creator<message_code_ping>(),
                                            &message_code_ping::saver);
        message_code_ping* pmsgcode =
                static_cast<message_code_ping*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_pong::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_pong::rtt,
                                            message_code_creator<message_code_pong>(),
                                            &message_code_pong::saver);
        message_code_pong* pmsgcode =
                static_cast<message_code_pong*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_error::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_error::rtt,
                                            message_code_creator<message_code_error>(),
                                            &message_code_error::saver);
        message_code_error* pmsgcode =
                static_cast<message_code_error*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_time_out::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_time_out::rtt,
                                            message_code_creator<message_code_time_out>(),
                                            &message_code_time_out::saver);
        message_code_time_out* pmsgcode =
                static_cast<message_code_time_out*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_get_peers::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_get_peers::rtt,
                                            message_code_creator<message_code_get_peers>(),
                                            &message_code_get_peers::saver);
        message_code_get_peers* pmsgcode =
                static_cast<message_code_get_peers*>(return_value.pmsg.get());
        code = analyze_json(*pmsgcode, pexp);
    }
        break;
    case message_code_peer_info::rtt:
    {
        return_value =
                beltpp::detail::pmsg_all(   message_code_peer_info::rtt,
                                            message_code_creator<message_code_peer_info>(),
                                            &message_code_peer_info::saver);
        message_code_peer_info* pmsgcode =
                static_cast<message_code_peer_info*>(return_value.pmsg.get());
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
