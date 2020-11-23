#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h> //ushort

/*--------------------------------------------------------------------*/
#define MAXSTRING   1024
#define MAXNODES    32
#define MAXPORTS    32
#define MAXFWDENTS  32
#define BUF_SIZE    2000
#define BUF_SIZE2   50
#define ADDR_SIZE   50
#define MASK_SIZE   32
#define NAME_SIZE   50
#define ADDR_NUM    10
#define MAX_CONFIG_INTERVAL 30

#define MAC_FILE  "mac-addr.conf"
#define IP_FILE "ip-addr.conf"
#define GW_FILE "gateway.conf"
/*--------------------------------------------------------------------*/

/* data type of IP packet's payload */
enum DATA_TYPE
{
  DATA_DV = 0,  //distance-vector packet
  DATA_CHAT = 1 //chatting packet
};

/* hub's status */
enum HUB_STATUS
{
  HUB_DOWN = 0,
  HUB_UP = 1
};

/* station kind */
enum STATION_KIND
{
  STATION_HUB = 0,   //hub
  STATION_HOST = 1,  //host
  STATION_ROUTER = 2 //router
};

/*--------------------------------------------------------------------*/

/* hardware address is 6 bytes */
typedef unsigned char HwAddr[6];

/* bcast and mcast addresses */
extern HwAddr BCASTADDR; //MAC broadcast address
extern HwAddr MCASTADDR; //MAC multicast address

extern in_addr_t IP_BCASTADDR; //IP broadcast address
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/* structure of an ethernet pkt */
typedef struct __ethpkt 
{
  /* destination address */
  HwAddr dst;

  /* source address */
  HwAddr src;

  /* length of packet */
  ushort len;

  /* actual payload */
  char * dat;

} EthPkt;

/* structure of an IP pkt */
typedef struct __ippkt
{
  /* destination address */
  in_addr_t dst;

  /* source address */
  in_addr_t src;

  /* length of packet */
  ushort len;

  /* type of packet */
  u_char type;

  /* actual payload */
  char * dat;
} IPPkt;
/*--------------------------------------------------------------------*/

/* structure of a DV message */
typedef struct __dvmsg
{
  /* command */
  ushort cmd;

  /* length of msg : its unit is byte and it indicates the length of the address entries */
  ushort len;

  /* payload containing the records of (IP network adddress, subnet mask, hop count) */
  char * dat;
} DVMsg;

/* structure of DV entry for network information */
typedef struct __dventry
{
  in_addr_t dest; //destination IP network address
  int mask; //net mask of destination IP network address
  int hop; //hop count from the router to the destination network
} DVEnt;

/*--------------------------------------------------------------------*/
/* recv an ether packet */
extern EthPkt *recvethpkt(int sd);

/* send an ether packet */
extern int sendethpkt(int sd, EthPkt *ethpkt);

/* output ether packet contents */
extern void dumpethpkt(EthPkt *ethpkt);

/* free up space allocated for ethpkt */
extern void freeethpkt(EthPkt *ethpkt);
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/* recv an IP packet */
extern IPPkt *recvippkt(int sd);

/* send an IP packet */
extern int sendippkt(int sd, IPPkt *ippkt);

/* output IP packet contents */
extern void dumpippkt(IPPkt *ippkt);

/* free up space allocated for ippkt */
extern void freeippkt(IPPkt *ippkt);
/*----------------------------------------------------------------*/

/* convert name to hardware addr */
extern int nametohwaddr(char *name, HwAddr addr);

/* return name given hardware addr */
extern int hwaddrtoname(HwAddr addr, char *name);

/* convert string to hardware address */
extern int strtohwaddr(char *str, HwAddr adr);

/* convert hardware address to string */
extern int hwaddrtostr(HwAddr adr, char *str);

/* compare two hardware addresses */
extern int hwaddrcmp(HwAddr adr1, HwAddr adr2);

/* copy hard address from adr2 to adr1 */
extern int hwaddrcpy(HwAddr adr1, HwAddr adr2);
/*----------------------------------------------------------------*/

#endif
