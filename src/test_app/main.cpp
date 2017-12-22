#include <belt.pp/message.hpp>
#include <belt.pp/messagecodes.hpp>
#include <json/json.hpp>
#include <belt.pp/parser.hpp>
#include <belt.pp/meta.hpp>

#include <iostream>
#include <string>

using message_list =
beltpp::typelist::type_list<
class message_blockchain_issue_block>;

using event_list =
beltpp::typelist::type_list<
class event_network_transaction_call,
int,
char
>;

class event_network_transaction_call :
        public beltpp::message_code<event_network_transaction_call, event_list>
{};

class message_blockchain_issue_block :
        public beltpp::message_code<message_blockchain_issue_block, message_list>
{};

size_t a = beltpp::typelist::type_list_index<event_network_transaction_call, event_list>::value;

using discard_lexers =
beltpp::typelist::type_list<
class white_space_lexer
>;

using operator_lexers =
beltpp::typelist::type_list<
class operator_plus_lexer,
class operator_minus_lexer
//class operator_multiply,
//class operator_divide
>;

using value_lexers =
beltpp::typelist::type_list<
class value_string_lexer,
class value_number_lexer
//class value_bool,
//class value_null
>;

using scope_lexers =
beltpp::typelist::type_list<
class scope_braces_lexer
//class scope_parentheses
>;

class white_space_lexer :
        public beltpp::discard_lexer_base<white_space_lexer,
                                            discard_lexers,
                                            beltpp::standard_white_space_set>
{};

class operator_plus_lexer :
        public beltpp::operator_lexer_base<operator_plus_lexer,
                                            operator_lexers,
                                            beltpp::standard_operator_set>
{
public:
    bool final_check(std::string::iterator it_begin,
                     std::string::iterator it_end) const
    {
        return std::string(it_begin, it_end) == "+";
    }
};
class operator_minus_lexer :
        public beltpp::operator_lexer_base<operator_minus_lexer,
                                            operator_lexers,
                                            beltpp::standard_operator_set>
{
public:
    bool final_check(std::string::iterator it_begin,
                     std::string::iterator it_end) const
    {
        return std::string(it_begin, it_end) == "-";
    }
};

class value_string_lexer :
        public beltpp::value_lexer_base<value_string_lexer, value_lexers>
{
    enum states
    {
        state_none = 0x0,
        state_single_quote_open = 0x01,
        state_double_quote_open = 0x02,
        state_escape_symbol = 0x04
    };
    int state = state_none;
    size_t index = -1;
public:
    std::pair<bool, bool> check(char ch)
    {
        ++index;
        if (0 == index && ch == '\'')
        {
            state |= state_single_quote_open;
            return std::make_pair(true, false);
        }
        if (0 == index && ch == '\"')
        {
            state |= state_double_quote_open;
            return std::make_pair(true, false);
        }
        if (0 == index && ch != '\'' && ch != '\"')
            return std::make_pair(false, false);
        if (0 != index && ch == '\'' &&
            (state & state_single_quote_open) &&
            !(state & state_escape_symbol))
            return std::make_pair(true, true);
        if (0 != index && ch == '\"' &&
            (state & state_double_quote_open) &&
            !(state & state_escape_symbol))
            return std::make_pair(true, true);
        if (0 != index && ch == '\\' &&
            !(state & state_escape_symbol))
        {
            state |= state_escape_symbol;
            return std::make_pair(true, false);
        }
        state &= ~state_escape_symbol;
        return std::make_pair(true, false);
    }
};

double stod(std::string const& value, size_t& pos)
{
    double result;
    try
    {
        result = std::stod(value, &pos);
    }
    catch(...)
    {
        pos = 0;
    }

    return result;
}
uint64_t stoull(std::string const& value, size_t& pos)
{
    uint64_t result = 0;
    try
    {
        result = std::stoull(value, &pos);
    }
    catch(...)
    {
        pos = 0;
    }

    return result;
}

class value_number_lexer :
        public beltpp::value_lexer_base<value_number_lexer, value_lexers>
{
    size_t state_dot_count = 0;
public:
    std::pair<bool, bool> check(char ch)
    {
        std::string str_full_set = "0123456789.elx";
        if (str_full_set.find(ch) != std::string::npos)
        {
            if (0 == state_dot_count && ch == '.')
                state_dot_count = 1;
            else if (ch == '.')
                return std::make_pair(false, false);

            return std::make_pair(true, false);
        }
        return std::make_pair(false, false);
    }

    bool final_check(std::string::iterator it_begin,
                     std::string::iterator it_end) const
    {
        size_t pos = 0;
        std::string value(it_begin, it_end);
        stod(value, pos);
        if (pos != value.length())
            stoull(value, pos);

        if (pos != value.length())
            return false;
        return true;
    }
};

class scope_braces_lexer :
        public beltpp::scope_lexer_base<scope_braces_lexer, scope_lexers>
{
    bool state_must_open = false;
public:
    bool is_close = false;

    std::pair<bool, bool> check(char ch)
    {
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z'))
        {
            state_must_open = true;
            return std::make_pair(true, false);
        }
        if (state_must_open && ch == '}')
            return std::make_pair(false, false);
        if (false == state_must_open && ch == '}')
        {
            is_close = true;
            return std::make_pair(true, true);
        }
        if (ch == '{')
            return std::make_pair(true, true);

        return std::make_pair(false, false);
    }

    bool final_check(std::string::iterator it_begin,
                     std::string::iterator it_end) const
    {
        --it_end;
        if (*it_end == '{' || *it_end == '}')
            return true;
        return false;
    }
};

int main()
{
    int aa = 0;
    using token_type = beltpp::typelist::type_list_get<1, operator_lexers>::type;
    token_type tt;
    operator_minus_lexer tt1;
    tt1 = tt;
    ++aa;
    std::unique_ptr<
            beltpp::expression_tree<operator_lexers,
                                    value_lexers,
                                    scope_lexers,
                                    discard_lexers,
                                    std::string>> a;
    std::string test("\'+-\'+0x2 + s{- 0x3}\"str\\\"ing\'\"-\'k\\\'\"l\'");
    auto it_begin = test.begin();
    auto it_end = test.end();
    auto it_begin_keep = it_begin;
    while (beltpp::parse(a, it_begin, it_end))
    {
        if (it_begin == it_begin_keep)
            break;
        else
        {
            std::cout << std::string(it_begin_keep, it_begin) << std::endl;
            it_begin_keep = it_begin;
        }
    }
    //auto test = 1;
    nlohmann::json var = {{"name", "niels"},
                          {"last_name","lohmann"}};
    beltpp::message msg;
    //msg.set();
    std::cout << msg.type() << std::endl;
    std::cout << var << std::endl;
    return 0;
}
