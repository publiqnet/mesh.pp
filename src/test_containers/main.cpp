#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/scope_helper.hpp>

#include <mesh.pp/fileutility.hpp>

#include <iostream>
#include <unordered_set>

using std::string;
using beltpp::packet;
using std::cout;
using std::cin;
using std::endl;

using namespace test_containers;


inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    test_containers::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

int main(int argc, char* argv[])
{
    try
    {
        meshpp::map_loader<Value> map("map", "/Users/tigran/publiq.pp1/map", get_putl());
        Value v;
        v.num = 0;
        map.at("0").num = 30;
        map.at("1").num = 20;
        map.insert("2", v);
        map.save();
        map.commit();
    }
    catch(std::exception const& ex)
    {
        std::cout << "exception: " << ex.what() << std::endl;
        return -1;
    }
    catch(...)
    {
        std::cout << "too well done ...\nthat was an exception\n";
        return -1;
    }
    return 0;
}
