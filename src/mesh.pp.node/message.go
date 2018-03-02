package beltpp

type message_join struct {}
type message_drop struct {}
type message_error struct {}
type message_timer_out struct {}

type message_ip_destination struct
{
    port uint16
    address string
}
type message_ip_address struct
{
    ip_type int32
    local message_ip_destination
    remote message_ip_destination
}
type message_ping struct
{
    nodeid string
}
type message_pong struct
{
    nodeid string
}
type message_find_node struct
{
    nodeid string
}
type message_node_details struct
{
    nodeid string
}
type message_introduce_to struct
{
    nodeid string
}
type message_open_connection_with struct
{
    addr message_ip_address
}
type message_hello struct
{
    value []string
}
