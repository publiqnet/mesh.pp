
#define _VERSION_ 2

#if (_VERSION_ == 1)
#include <belt.pp/parser.hpp>

#include <iostream>
#include <string>

using operator_lexers =
beltpp::typelist::type_list<
class operator_plus_lexer,
class operator_minus_lexer,
class operator_multiply_lexer,
class operator_brakets_lexer,
class value_number_lexer,
class white_space_lexer
>;

class operator_plus_lexer :
        public beltpp::operator_lexer_base<operator_plus_lexer,
                                            operator_lexers>
{
public:
    size_t right = 1;
    size_t left_max = -1;
    size_t left_min = 0;
    enum { grow_priority = 1 };

    beltpp::e_three_state_result check(char ch)
    {
        return beltpp::standard_operator_check<beltpp::standard_operator_set<void>>(ch);
    }

    template <typename T_iterator>
    bool final_check(T_iterator const& it_begin,
                     T_iterator const& it_end) const
    {
        return std::string(it_begin, it_end) == "+";
    }
};
class operator_minus_lexer :
        public beltpp::operator_lexer_base<operator_minus_lexer,
                                            operator_lexers>
{
public:
    size_t right = 3;
    size_t left_max = -1;
    size_t left_min = 1;
    enum { grow_priority = 1 };

    beltpp::e_three_state_result check(char ch)
    {
        return beltpp::standard_operator_check<beltpp::standard_operator_set<void>>(ch);
    }

    template <typename T_iterator>
    bool final_check(T_iterator const& it_begin,
                     T_iterator const& it_end) const
    {
        return std::string(it_begin, it_end) == "-";
    }
};
class operator_multiply_lexer :
        public beltpp::operator_lexer_base<operator_multiply_lexer,
                                            operator_lexers>
{
public:
    size_t right = 1;
    size_t left_max = 1;
    size_t left_min = 1;
    enum { grow_priority = 1 };

    beltpp::e_three_state_result check(char ch)
    {
        return beltpp::standard_operator_check<beltpp::standard_operator_set<void>>(ch);
    }

    template <typename T_iterator>
    bool final_check(T_iterator const& it_begin,
                     T_iterator const& it_end) const
    {
        return std::string(it_begin, it_end) == "*";
    }
};
class operator_brakets_lexer :
        public beltpp::operator_lexer_base<operator_brakets_lexer,
                                            operator_lexers>
{
    bool state_must_open = false;
public:
    size_t right = 1;
    size_t left_max = 1;
    size_t left_min = 1;
    enum { grow_priority = 1 };

    beltpp::e_three_state_result check(char ch)
    {
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z'))
        {
            state_must_open = true;
            return beltpp::e_three_state_result::attempt;
        }
        if (state_must_open && ch == '}')
            return beltpp::e_three_state_result::error;
        if (false == state_must_open && ch == '}')
        {
            right = 0;
            left_min = 0;
            left_max = 1;
            return beltpp::e_three_state_result::success;
        }
        if (ch == '{')
        {
            right = 1;
            left_min = 0;
            left_max = 0;
            return beltpp::e_three_state_result::success;
        }

        return beltpp::e_three_state_result::error;
    }

    template <typename T_iterator>
    bool final_check(T_iterator const& it_begin,
                     T_iterator const& it_end) const
    {
        std::string value(it_begin, it_end);
        return (value == "{" || value == "}");
    }
};

class value_number_lexer :
        public beltpp::value_lexer_base<value_number_lexer,
                                        operator_lexers>
{
    std::string value;
private:
    bool _check(std::string const& v) const
    {
        size_t pos = 0;
        beltpp::stod(v, pos);
        if (pos != v.length())
            beltpp::stoll(v, pos);

        if (pos != v.length())
            return false;
        return true;
    }
public:
    beltpp::e_three_state_result check(char ch)
    {
        value += ch;
        if ("." == value || "-" == value || "-." == value)
            return beltpp::e_three_state_result::attempt;
        else if (_check(value))
            return beltpp::e_three_state_result::attempt;
        else
            return beltpp::e_three_state_result::error;
    }

    template <typename T_iterator>
    bool final_check(T_iterator const& it_begin,
                     T_iterator const& it_end) const
    {
        return _check(std::string(it_begin, it_end));
    }

    bool scan_beyond() const
    {
        return false;
    }
};


class white_space_lexer :
        public beltpp::discard_lexer_base<white_space_lexer,
                                            operator_lexers,
                                            beltpp::standard_white_space_set<void>>
{};

int main()
{
    std::unique_ptr<
            beltpp::expression_tree<operator_lexers,
                                    std::string>> ptr_expression;
    //std::string test("\'+-\'+0x2 + s{- 0x3}\"str\\\"ing\'\"-\'k\\\'\"l\'");
    //std::string test("{{{{{{}}}}}+\'192.168.1.1\'-1 2 3}");
    std::string test("1*2-3+4 4*1 5+6+7");
    //std::string test("1+{2}+ + + 3+4+5");
    //std::string test("+");
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

    std::cout << "=====\n";
    while (p_iterator)
    {
        if (track.size() == depth)
            track.push_back(0);

        if (track.size() == depth + 1)
        {
            for (size_t index = 0; index != depth; ++index)
                std::cout << '.';
            std::cout << p_iterator->lexem.value;
            if (p_iterator->lexem.right > 0)
                std::cout << " !!!! ";
            std::cout << std::endl;
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

#elif (_VERSION_ == 2)

#include <belt.pp/json.hpp>

#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;

int main()
{
    size_t const limit = 2;
    beltpp::json::ptr_expression_tree pexp;
    beltpp::json::expression_tree* proot = nullptr;
    {
    string test1 = "{\"rtt\":{},\"message\":{\"rtt\":4},\"\" :1";
    auto it_begin = test1.begin();
    auto it_end = test1.end();

    auto code = beltpp::json::parse_stream(pexp, it_begin, it_end, limit, 10, proot);

    cout << &(*it_begin) << endl;
    if (beltpp::e_three_state_result::error == code)
        cout << "test1 error" << endl;
    else
    {
        cout << "<===" << endl;
        cout << beltpp::dump(proot) << endl;
        cout << "===>" << endl;
        if (beltpp::e_three_state_result::success == code)
            return 0;
    }
    }

    {
    string test1 = "21}";
    auto it_begin = test1.begin();
    auto it_end = test1.end();

    auto code = beltpp::json::parse_stream(pexp, it_begin, it_end, limit, 10, proot);

    cout << &(*it_begin) << endl;
    if (beltpp::e_three_state_result::error == code)
        cout << "test2 error" << endl;
    else
    {
        cout << "<===" << endl;
        cout << beltpp::dump(proot) << endl;
        cout << "===>" << endl;
        if (beltpp::e_three_state_result::success == code)
            return 0;
    }
    }

    return 0;
}

#endif
