#include <iostream>

#include <belt.pp/message.hpp>
#include <json/json.hpp>

int main()
{
    //auto test = 1;
    nlohmann::json var = {{"name", "niels"},
                          {"last_name","lohmann"}};
    beltpp::message msg;
    std::cout << msg.type() << std::endl;
    std::cout << var << std::endl;
    return 0;
}
