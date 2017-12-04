#include <iostream>

#include <belt.pp/message.hpp>

int main()
{
   //auto test = 1;
   beltpp::message msg;
   std::cout << msg.type() << std::endl;
   return 0;
}
