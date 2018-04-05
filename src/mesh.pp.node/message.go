module meshpp_message
{
type message_join {}
type message_drop {}
type message_error {}
type message_timer_out {}

class message_ip_destination
{
    UInt16 port
    String address
}
class message_ip_address
{
    Int32 ip_type
    message_ip_destination local
    message_ip_destination remote
}
class message_ping
{
    String nodeid
}
class message_pong
{
    String nodeid
}
class message_find_node
{
    String nodeid
}
class message_node_details
{
    String origin
    Array String nodeid
}
class message_introduce_to
{
    String nodeid
}
class message_open_connection_with
{
    message_ip_address addr
}
class message_hello
{
    Array String value
    Hash String String hash_table
}
class message_hello_container
{
    Array message_hello lst
    Hash message_hello message_hello mp
    Hash Int Int mp2
    Array TimePoint tm
    Object obj
}
class message_string
{
    String message
}
class message_stamp
{
    Object obj
    TimePoint stamp
}
class message_lookup_node
{
    String nodeid
}
}
