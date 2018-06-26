#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/event.hpp>

#include <iostream>
#include <unordered_set>

using std::string;
using std::vector;
using std::pair;
using beltpp::ip_address;
using beltpp::packet;
using packets = beltpp::socket::packets;
using peer_id = beltpp::socket::peer_id;
using peer_ids = beltpp::socket::peer_ids;
using std::cout;
using std::cin;
using std::endl;
using std::unordered_set;

using namespace meshpp_message;

//  MSVS does not instansiate template function only because its address
//  is needed, so let's force it
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<message_error>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<message_join>();
template beltpp::void_unique_ptr beltpp::new_void_unique_ptr<message_drop>();

using sf = beltpp::socket_family_t<
    message_error::rtt,
    message_join::rtt,
    message_drop::rtt,
    &beltpp::new_void_unique_ptr<message_error>,
    &beltpp::new_void_unique_ptr<message_join>,
    &beltpp::new_void_unique_ptr<message_drop>,
    &message_error::saver,
    &message_join::saver,
    &message_drop::saver,
    &message_list_load
>;


int main(int argc, char* argv[])
{
    try
    {
        beltpp::event_handler eh;
        beltpp::socket sk = beltpp::getsocket<sf>(eh);
        //eh.set_timer(std::chrono::seconds(10));

        ip_address addr("127.0.0.1", 5555);
        addr.ip_type = ip_address::e_type::ipv4;


        unordered_set<beltpp::ievent_item const*> set;

        if (argc == 1)
        {
            sk.listen(addr);
            while (true)
            {
                try
                {
                    eh.wait(set);

                    peer_id peer;
                    packets received_packets = sk.receive(peer);

                    for (auto const& received_packet : received_packets)
                    {
                        switch (received_packet.type())
                        {
                        case message_join::rtt:
                        {
                            cout << "peer " << peer << " joined" << endl;
                            break;
                        }
                        case message_drop::rtt:
                            cout << "peer " << peer << " dropped" << endl;
                            break;
                        case message_error::rtt:
                            cout << "error from peer " << peer << endl;
                            break;
                        case Sum::rtt:
                        {
                            Sum msg_sum;
                            received_packet.get(msg_sum);
                            SumResult msg_result;
                            msg_result.value = msg_sum.first + msg_sum.second;

                            cout << msg_sum.first << " + " << msg_sum.second << endl;
                            sk.send(peer, msg_result);
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
                catch(std::exception const& ex)
                {
                    std::cout << "exception: " << ex.what() << std::endl;
                }
                catch(...)
                {
                    std::cout << "too well done ...\nthat was an exception\n";
                }
            }
        }
        else
        {
            sk.open(addr);
            peer_id connected_peer;
            while (true)
            {
                eh.wait(set);
                peer_id peer;
                packets received_packets = sk.receive(peer);


                if (!peer.empty())
                    connected_peer = peer;
                if (connected_peer.empty())
                    return 0;

                if (received_packets.empty())
                    continue;

                if (received_packets.front().type() == SumResult::rtt)
                {
                    SumResult msg_sum;
                    received_packets.front().get(msg_sum);

                    cout << msg_sum.value << endl;
                }


                double a = 1, b = 3;
                cin >> a >> b;
                Sum msg_sum;
                msg_sum.first = a;
                msg_sum.second = b;
                sk.send(peer, msg_sum);};

        }


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
