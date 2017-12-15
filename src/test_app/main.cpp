#include <iostream>

#include <belt.pp/message.hpp>
#include <belt.pp/messagecodes.hpp>
#include <json/json.hpp>

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

int main()
{
    //auto test = 1;
    nlohmann::json var = {{"name", "niels"},
                          {"last_name","lohmann"}};
    beltpp::message msg;
    //msg.set();
    std::cout << msg.type() << std::endl;
    std::cout << var << std::endl;
    return 0;
}
