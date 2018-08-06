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

using sf = beltpp::socket_family_t<&message_list_load>;

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
                        case beltpp::isocket_join::rtt:
                        {
                            cout << "peer " << peer << " joined" << endl;
                            break;
                        }
                        case beltpp::isocket_drop::rtt:
                            cout << "peer " << peer << " dropped" << endl;
                            break;
                        case beltpp::isocket_protocol_error::rtt:
                        {
                            beltpp::isocket_protocol_error msg;
                            received_packet.get(msg);
                            cout << "error from peer " << peer << endl;
                            cout << msg.buffer << endl;
                            break;
                        }
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
        else if (argc == 2)
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
        else
        {
            unordered_set<string> to_open;

            ip_address addr;
            addr.from_string(argv[2]);
            peer_ids peers_to_open = sk.open(addr);
            cout << "will try to open" << endl;
            for (auto const& item : peers_to_open)
            {
                to_open.insert(item);
                cout << "    " << item << endl;
            }
            cout << endl;

            while (true)
            {
                eh.wait(set);
                peer_id peer;
                packets received_packets = sk.receive(peer);
                if (received_packets.empty())
                    continue;

                assert(received_packets.size() == 1);
                auto& packet = received_packets.front();

                if (packet.type() == beltpp::isocket_join::rtt)
                {
                    cout << "joined: " << peer << endl;
                    sk.send(peer, beltpp::isocket_drop());
                }
                else if (packet.type() == beltpp::isocket_open_refused::rtt)
                {
                    beltpp::isocket_open_refused msg;
                    packet.get(msg);
                    cout << "open refused: " << peer << ": " << msg.reason << endl;
                }
                else if (packet.type() == beltpp::isocket_open_error::rtt)
                {
                    beltpp::isocket_open_error msg;
                    packet.get(msg);
                    cout << "open error: " << peer << ": " << msg.reason << endl;
                }
                else
                {
                    assert(false);
                }

                to_open.erase(peer);

                if (to_open.empty())
                    break;
            }
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
