/** File name: dist-vec.h
    Description: implementation of distance vector routing protocol
    Date: 03/11/2020
    Maker: Jaehoon Jeong, pauljeong@skku.edu
*/

#ifndef __DIST_VECTOR_H__
#define __DIST_VECTOR_H__

#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "common.h"

#define RT_TABLE_SIZE 50
//size of routing table

#define NET_TABLE_SIZE 30
//size of network address table

#define FW_TABLE_SIZE 30
//size of forwarding table

#define PORT_TABLE_SIZE 10
//size of port table

#define ITF_NAME_SIZE 20
//size of interface name

/* status of routing table entry */
enum RTE_STATUS
{
  RTE_DOWN = 0,
  RTE_UP = 1
};

/* DV Command for DV Exchange Message */
enum DV_COMMAND
{
  DV_ADVERTISE = 0, //DV Advertisement Message
  DV_BREAKAGE = 1   //DV Link Breakage Message
};

typedef struct _rt_table_entry
{
  in_addr_t dest; //destination IP network address
  int mask; //subnet mask of destination IP network address
  in_addr_t next; //IP address of next-hop router toward destination network
  int hop; //hop count from the router to the destination network
  int itf; //network interface (port) attached toward network
  char itf_name[ITF_NAME_SIZE]; //interface name
  int status; //the status of the entry = {RTE_DOWN, RTE_UP}
  long time; //the last refreshed time for this entry
} rt_table_entry;

typedef struct _net_table_entry
{
  in_addr_t net; //IP network address
  int mask; //subnet mask of IP network address  
} net_table_entry;

/* forwarding table entry */
typedef struct _fw_table_entry
{
  in_addr_t dest; //destination IP network address
  int mask; //subnet mask of destination IP network address
  in_addr_t next; //IP address of next-hop router toward destination network
  int itf; //corresponding socket
  char itf_name[ITF_NAME_SIZE]; //interface name
  int flag; //indicate whether this fw entry is valid or not; if flag = 1, the entry is valid,
            //and so the entry can be used for forwarding IP packet; otherwise, entry is invalid.
} fw_table_entry;

typedef struct _dv_entry
{
  in_addr_t dest; //destination IP network address
  int mask; //net mask of destination IP network address
  int hop; //hop count from the router to the destination network
} dv_entry;

/* interface port */
typedef struct _port_table_entry
{
  int itf; //socket for the interface port
  char itf_name[ITF_NAME_SIZE]; //interface name
} port_table_entry;

char* dv_get_itf_name(int sock); //return the interface name corresponding to incoming socket (sock)

int dv_init_tables(in_addr_t* addr, int* mask, int addr_num, int* sock); //initialize g_rt_table, g_net_table, and g_fw_table with the router's network information

int dv_update_rt_table(int sock, in_addr_t neighbor, dv_entry* dv, int dv_entry_num); //update g_rt_table with dv entries sent by neighbor which are reachable network via the neighbor

int dv_update_rt_table_for_link_breakage(int sock, in_addr_t neighbor, dv_entry* dv, int dv_entry_num); //update g_rt_table with dv entry sent by neighbor which becomes unreachable network due the link breakage (e.g., hub is down)

int dv_update_fw_table(); //update forwarding table (g_fw_table) with the routing table (g_rt_table)

int dv_update_routing_info(int sock,
 char* dat, int dat_len, in_addr_t src); //update routing table and forwarding table

void dv_update_tables_for_timeout(long curtime, int myconfigint); //update the routing table and forwarding table for timeout

int dv_forward(IPPkt* ippkt); //forward the packet to the appropriate next hop router or host

void dv_show_routing_table(); //show routing table

void dv_show_forwarding_table(); //show forwarding table

int dv_get_sock_for_destination(int sock, in_addr_t src, in_addr_t dst); //return an appropriate socket for destination address dst with the forwarding table

int dv_ipaddr_to_hwaddr(in_addr_t ippkt_dst, HwAddr ethpkt_dst); //convert the dst IP address into next hop's MAC address

int dv_broadcast_dv_message(); //broadcast the routing information with DV exchange message

int dv_broadcast_dv_message_for_link_breakage(); //broadcast the routing information with DV exchange message containing the network address related to the link which is broken due to a hub crash.

#endif
