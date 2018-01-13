#include <belt.pp/message_global.hpp>
#include <belt.pp/json.hpp>
#include <belt.pp/isocket.hpp>
#include <string>
#include <unordered_map>

namespace beltpp
{
namespace detail
{
bool analyze_json_object(beltpp::json::expression_tree* pexp,
                         size_t& rtt);
bool analyze_json_object(beltpp::json::expression_tree* pexp,
                         beltpp::detail::pmsg_all& return_value);
bool analyze_json_common(size_t& rtt,
                         beltpp::json::expression_tree* pexp,
                         std::unordered_map<std::string, beltpp::json::expression_tree*>& members);
bool analyze_colon(beltpp::json::expression_tree* pexp,
                   size_t& rtt);
bool analyze_colon(beltpp::json::expression_tree* pexp,
                   std::unordered_map<std::string, beltpp::json::expression_tree*>& members);
}
beltpp::detail::pmsg_all message_list_load(
        beltpp::iterator_wrapper<char const>& iter_scan_begin,
        beltpp::iterator_wrapper<char const> const& iter_scan_end)
{
    auto const it_backup = iter_scan_begin;

    beltpp::json::ptr_expression_tree pexp;
    beltpp::json::expression_tree* proot = nullptr;

    beltpp::detail::pmsg_all return_value (size_t(-1),
                                           detail::ptr_msg(nullptr, [](void*&){}),
                                           nullptr);

    auto code = beltpp::json::parse_stream(pexp, iter_scan_begin,
                                           iter_scan_end, 1024*1024, proot);

    if (beltpp::e_three_state_result::success == code &&
        nullptr == pexp)
    {
        assert(false);
        //  this should not happen, but since this is a
        //  network protocol code, let's assume there is a bug
        //  and not crash, not throw, only report an error
        code = beltpp::e_three_state_result::error;
    }
    else if (beltpp::e_three_state_result::success == code &&
             false ==
             detail::analyze_json_object(pexp.get(),
                                         return_value.rtt))
    {
        //  this can happen if a different application is
        //  trying to fool us, by sending incorrect json structures
        code = beltpp::e_three_state_result::error;
    }
    else if (beltpp::e_three_state_result::success == code &&
             false == detail::analyze_json_object(pexp.get(), return_value))
    {
        code = beltpp::e_three_state_result::error;
    }
    //
    //
    if (beltpp::e_three_state_result::error == code)
    {
        iter_scan_begin = iter_scan_end;
         return_value = beltpp::detail::pmsg_all(0,
                                                 detail::ptr_msg(nullptr, [](void*&){}),
                                                 nullptr);
    }
    else if (beltpp::e_three_state_result::attempt == code)
    {   //  revert the cursor, so everything will be rescaned
        //  once there is more data to scan
        //  in future may implement persistent state, so rescan will not
        //  be needed
        iter_scan_begin = it_backup;
        beltpp::detail::pmsg_all return_value (size_t(-1),
                                               detail::ptr_msg(nullptr, [](void*&){}),
                                               nullptr);
    }

    return return_value;
}

template <typename Tself, typename Tother>
void assign(Tself& self, Tother const& other)
{
    self = static_cast<Tself>(other);
}

namespace detail
{
std::string saver(int value)
{
    return std::to_string(value);
}
std::string saver(std::string const& value)
{
    return "\"" + value + "\"";
}
bool analyze_json(int& value,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    if (nullptr == pexp ||
        pexp->lexem.rtt != json::value_number::rtt)
        code = false;
    else
    {
        std::string const& str_value = pexp->lexem.value;
        size_t pos;
        value = beltpp::stoll(str_value, pos);

        if (pos != str_value.length())
            code = false;
    }

    return code;
}

bool analyze_json(std::string& value,
                  beltpp::json::expression_tree* pexp)
{
    bool code = true;
    if (nullptr == pexp ||
        pexp->lexem.rtt != json::value_string::rtt)
        code = false;
    else
    {
        value = pexp->lexem.value;
        value = value.substr(1, value.length() - 2);
    }

    return code;
}
}

using std::string;
#include "message.gen.hpp"

namespace detail
{
bool analyze_json_common(size_t& rtt,
                         beltpp::json::expression_tree* pexp,
                         std::unordered_map<std::string, beltpp::json::expression_tree*>& members)
{
    bool code = false;

    if (nullptr == pexp ||
        pexp->lexem.rtt != beltpp::json::scope_brace::rtt ||
        1 != pexp->children.size())
        code = false;
    else if (pexp->children.front() &&
             pexp->children.front()->lexem.rtt ==
             beltpp::json::operator_colon::rtt)
    {
        if (analyze_colon(pexp->children.front(), rtt))
            code = true;
        else
            code = false;
    }
    else if (nullptr == pexp->children.front() ||
             pexp->children.front()->lexem.rtt !=
             beltpp::json::operator_comma::rtt ||
             pexp->children.front()->children.empty())
    {
        code = false;
    }
    else
    {
        code = false;
        auto pcomma = pexp->children.front();
        for (auto item : pcomma->children)
        {
            if (false == analyze_colon(item, rtt) &&
                false == analyze_colon(item, members))
                break;
        }

        if (members.size() + 1 == pexp->children.size())
            code = true;
    }

    return code;
}

bool analyze_json_object(beltpp::json::expression_tree* pexp,
                         size_t& rtt)
{
    bool code = false;
    rtt = -1;

    if (nullptr == pexp ||
        pexp->lexem.rtt != beltpp::json::scope_brace::rtt ||
        1 != pexp->children.size())
        code = false;
    else if (pexp->children.front() &&
             pexp->children.front()->lexem.rtt ==
             beltpp::json::operator_colon::rtt)
    {
        if (analyze_colon(pexp->children.front(), rtt))
            code = true;
        else
            code = false;
    }
    else if (nullptr == pexp->children.front() ||
             pexp->children.front()->lexem.rtt !=
             beltpp::json::operator_comma::rtt ||
             pexp->children.front()->children.empty())
    {
        code = false;
    }
    else
    {
        code = false;
        auto pcomma = pexp->children.front();
        for (auto item : pcomma->children)
        {
            if (analyze_colon(item, rtt))
            {
                code = true;
                break;
            }
        }
    }

    return code;
}

bool analyze_colon(beltpp::json::expression_tree* pexp,
                   size_t& rtt)
{
    rtt = -1;

    if (pexp &&
        pexp->lexem.rtt == beltpp::json::operator_colon::rtt &&
        2 == pexp->children.size() &&
        pexp->children.front() &&
        pexp->children.back() &&
        pexp->children.front()->lexem.rtt ==
        beltpp::json::value_string::rtt)
    {
        auto key = pexp->children.front()->lexem.value;
        if (key != "\"rtt\"")
        {
            //members.insert(std::make_pair(key, item->children.back()));
        }
        else if (pexp->children.back() &&
                 pexp->children.back()->lexem.rtt ==
                 beltpp::json::value_number::rtt)
        {
            size_t pos;
            auto const& value = pexp->children.back()->lexem.value;
            rtt = beltpp::stoull(value, pos);
            if (pos != value.size())
                rtt = -1;
        }
    }

    return (size_t(-1) != rtt);
}

bool analyze_colon(beltpp::json::expression_tree* pexp,
                   std::unordered_map<std::string, beltpp::json::expression_tree*>& members)
{
    bool code = false;

    if (pexp &&
        pexp->lexem.rtt == beltpp::json::operator_colon::rtt &&
        2 == pexp->children.size() &&
        pexp->children.front() &&
        pexp->children.back() &&
        pexp->children.front()->lexem.rtt ==
        beltpp::json::value_string::rtt)
    {
        auto key = pexp->children.front()->lexem.value;

        members.insert(std::make_pair(key, pexp->children.back()));
        code = true;
    }

    return code;
}
}
}
