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



using operator_lexers =
beltpp::typelist::type_list<
class operator_plus_lexer,
class operator_minus_lexer
//class operator_multiply,
//class operator_divide
>;

using scope_lexers =
beltpp::typelist::type_list<
class scope_braces_lexer
//class scope_parentheses
>;

class operator_plus_lexer :
        public beltpp::operator_lexer_base<operator_plus_lexer,
                                            operator_lexers,
                                            beltpp::standard_operator_set>
{
public:
    enum { grow_priority = 1 };
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
    enum { grow_priority = 0 };
    bool final_check(std::string::iterator it_begin,
                     std::string::iterator it_end) const
    {
        return std::string(it_begin, it_end) == "-";
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
    std::unique_ptr<
            beltpp::expression_tree<operator_lexers,
                                    beltpp::standard_value_lexers,
                                    scope_lexers,
                                    beltpp::standard_discard_lexers,
                                    std::string>> ptr_expression;
    //std::string test("\'+-\'+0x2 + s{- 0x3}\"str\\\"ing\'\"-\'k\\\'\"l\'");
    std::string test("{{{{{{}}}}}+\'192.168.1.1\'+\'13\'}");
    auto it_begin = test.begin();
    auto it_end = test.end();
    auto it_begin_keep = it_begin;
    while (beltpp::parse(ptr_expression, it_begin, it_end))
    {
        if (it_begin == it_begin_keep)
            break;
        else
        {
            std::cout << std::string(it_begin_keep, it_begin) << std::endl;
            it_begin_keep = it_begin;
        }
    }

    if (nullptr == ptr_expression)
        return 0;

    while (auto parent = ptr_expression->parent)
    {
        ptr_expression.release();
        ptr_expression.reset(parent);
    }

    auto p_iterator = ptr_expression.get();
    std::vector<size_t> track;
    size_t depth = 0;

    while (p_iterator)
    {
        if (track.size() == depth)
            track.push_back(0);

        if (track.size() == depth + 1)
        {
            for (size_t index = 0; index != depth; ++index)
                std::cout << ' ';
            std::cout << p_iterator->lexem.value << std::endl;
        }

        size_t next_child_index = -1;
        if (false == p_iterator->children.empty())
        {
            size_t childindex = 0;
            if (track.size() > depth + 1)
            {
                ++track[depth + 1];
                childindex = track[depth + 1];
            }

            if (childindex < p_iterator->children.size())
                next_child_index = childindex;
        }

        if (size_t(-1) != next_child_index)
        {
            p_iterator = p_iterator->children[next_child_index];
            ++depth;
            if (track.size() > depth + 1)
                track.resize(depth + 1);
        }
        else
        {
            p_iterator = p_iterator->parent;
            --depth;
        }
    }

    std::cout << "depth is " << ptr_expression->depth() << std::endl;

    return 0;
}
