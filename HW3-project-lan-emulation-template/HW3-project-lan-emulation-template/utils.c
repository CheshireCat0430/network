/*----------------------------------------------------------------*/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <time.h> 
#include <errno.h>
#include "common.h"
#include "dist-vec.h"

/* hardware broadcast address */
HwAddr BCASTADDR = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* hardware multicast address */
HwAddr MCASTADDR = { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* IP multicast address */
in_addr_t IP_BCASTADDR = 0xffffffff; //IP broadcast address

/*----------------------------------------------------------------*/


/*----------------------------------------------------------------*/
/* MAC address entry for node name */
typedef struct _mac_table_entry 
{
  char* name; //node name
  HwAddr addr; //node MAC address
} mac_table_entry;

/* IP address entry for LAN, router, or host name */
typedef struct _ip_table_entry 
{
  char* name; //name
  in_addr_t addr; //IP address
  int mask; //subnet mask
} ip_table_entry;

/* Default gateway entry for host */
typedef struct _gw_table_entry 
{
  char* host; //host name
  char* gw; //gateway name
} gw_table_entry;

/* LAN name entry for hub */
typedef struct _lan_table_entry
{
  int hubsock; //socket for a hub
  char lanname[NAME_SIZE]; //lan name
} lan_table_entry;

int g_station_kind; //let us know what kind of station the program is: {hub, host, router}

mac_table_entry g_mac_table[MAXNODES]; //MAC address table
int g_mac_table_size; //size of g_mac_table

ip_table_entry g_ip_table[MAXNODES]; //IP address table
int g_ip_table_size; //size of g_ip_table

gw_table_entry g_gw_table[MAXNODES]; //default gateway table
int g_gw_table_size; //size of g_gw_table

/* my MAC address */
HwAddr g_myhwaddr;

/* my IP address */
in_addr_t g_myipaddrs[ADDR_NUM];
int g_myipaddrs_num;

/* my subnet mask in network byte-order */
int g_mynetmasks[ADDR_NUM];
int g_mynetmasks_num;

/* my default gateway */
in_addr_t g_mygwaddr;

/* my hub's status */
int g_hub_status; //it is used to notify the IP stack or Ethernet stack that the hub is down when the received data size is zero.

/* list of LAN names corresponding to the sockets of hubs */
lan_table_entry g_lan_table[MAXNODES];
int g_lan_table_size;

/******************************************************************/
/* set station kind */
void set_station_kind(int kind)
{
  g_station_kind = kind;
} 

/* set hub's status to HUB_UP */
void set_hub_up()
{
  g_hub_status = HUB_UP;
}

/* set hub's status to HUB_DOWN */
void set_hub_down()
{
  g_hub_status = HUB_DOWN;
}

int hub_status()
{
  return g_hub_status;
}

/** manipulate the list of LAN names corresponding to the sockets of hubs */
int add_lanname_entry(int hubsock, char* lanname)
{ //add lanname entry to g_lan_table

  if(g_lan_table_size < MAXNODES)
  {
    g_lan_table[g_lan_table_size].hubsock = hubsock;
    strcpy(g_lan_table[g_lan_table_size].lanname, lanname);
  }
  else
  {

    printf("we cannot add a LAN entry any more since g_lan_table is full\n");
    return -1;
  }

  g_lan_table_size++;
  return 1;
}

int delete_lanname_entry(int hubsock)
{ //add lanname entry to g_lan_table
  int i;

  for(i = 0; i < g_lan_table_size; i++)
  {
    if(g_lan_table[i].hubsock == hubsock)
    {
      g_lan_table[i].hubsock = g_lan_table[g_lan_table_size-1].hubsock;
      strcpy(g_lan_table[i].lanname, g_lan_table[g_lan_table_size-1].lanname);
      g_lan_table_size--;
      break;
    } //end of if
  } //end of for

  return 1;
}

char* get_lanname(int hubsock)
{ //get the LAN name corresponding to hubsock
  int i;
  char* ptr = NULL;

  for(i = 0; i < g_lan_table_size; i++)
  {
    if(g_lan_table[i].hubsock == hubsock)
    {
      ptr = g_lan_table[i].lanname;
      break;
    } //end of if
  } //end of for

  return ptr;
}


/* Host registers its address information with g_myhwaddr and g_myipaddrs for checking if the received packet is mine or not and for sending my packet to the specified destination */
void set_host_addrinfo(HwAddr myhwaddr, in_addr_t myipaddr, int mynetmask, in_addr_t mygwaddr)
{
  memcpy(g_myhwaddr, myhwaddr, sizeof(HwAddr));
  g_myipaddrs[0] = myipaddr;
  g_myipaddrs_num = 1;
  g_mynetmasks[0] = mynetmask;
  g_mynetmasks_num = 1;
  g_mygwaddr = mygwaddr;
}

/* Router registers its address information with g_myhwaddr and g_myipaddrs for checking if the received packet is mine or not and for sending my packet to the specified destination */
void set_router_addrinfo(HwAddr myhwaddr, in_addr_t* myipaddrs, int myipaddrs_num, int* mynetmasks)
{
  int i;
  memcpy(g_myhwaddr, myhwaddr, sizeof(HwAddr));

  g_myipaddrs_num = 0;
  for(i=0; i < myipaddrs_num; i++)
  {
    g_myipaddrs[i] = myipaddrs[i];
    g_mynetmasks[i] = mynetmasks[i];
    g_myipaddrs_num++;
    g_mynetmasks_num++;
  } 

  g_mygwaddr = 0;
}

/* make subnet bit mask from /x notation in network byte-order */
int make_subnet_mask(char* mask)
{
  int subnet_mask = 0xffffffff;
  int bit_num;

  bit_num = atoi(mask);
  subnet_mask <<= (32 - bit_num);
  
  subnet_mask = htonl(subnet_mask);

  return subnet_mask;
}

/* init mac address configuration table */
int init_mac_table(char *macfile)
{
  FILE *fp;
  char name[MAXSTRING];
  char addr[MAXSTRING];

  /* open the file */
  fp = fopen(macfile, "r");
  if (!fp) {
    fprintf(stderr, "error : unable to open file '%s'\n",
	    macfile);
    return(0);
  }

  /* fill in node MAC addresses */
  g_mac_table_size = 0;
  while (fscanf(fp, "%s %s", name, addr) == 2) {
    g_mac_table[g_mac_table_size].name = strdup(name);
    /* strdup(name): return the pointer to the space allocated for the duplication of the string pointed by name. The  space is allocated by malloc(). */
    strtohwaddr(addr, g_mac_table[g_mac_table_size].addr);
    g_mac_table_size++;
  }
  return(1);
}

/* init default gateway configuration table */
int init_gw_table(char *gwfile)
{
  FILE *fp;
  char host_name[MAXSTRING];
  char gw_name[MAXSTRING];

  /* open the file */
  fp = fopen(gwfile, "r");
  if (!fp) {
    fprintf(stderr, "error : unable to open file '%s'\n",
	    gwfile);
    return(0);
  }

  /* fill in host's gateway DNS name */
  g_gw_table_size = 0;
  while (fscanf(fp, "%s %s", host_name, gw_name) == 2) {
    g_gw_table[g_gw_table_size].host = strdup(host_name);
    g_gw_table[g_gw_table_size].gw = strdup(gw_name);    
    g_gw_table_size++;
  }
  return(1);
}

/* init IP address configuration table */
int init_ip_table(char *ipfile)
{
  FILE *fp;
  char* token = NULL;
  char name[MAXSTRING];
  char addr[MAXSTRING];
  int mask; //subnet mask
  char buf[BUF_SIZE];
  char ip_addr_buf[ADDR_SIZE];
  char mask_buf[MASK_SIZE];
  char* ptr = NULL; //indicate the start of subnet mask

  /* open the file */
  fp = fopen(ipfile, "r");
  if (!fp) {
    fprintf(stderr, "error : unable to open file '%s'\n",
	    ipfile);
    return(0);
  }

  /* fill in IP address information */
  g_ip_table_size = 0;
  while (fgets(buf, sizeof(buf), fp) != NULL)
  {
    token = strtok(buf, " \t\n");
    if(token == NULL && g_ip_table_size == 0)
    {
      perror("init_ip_table(): token is NULL");
      exit(1);
    }
    else if(token == NULL && g_ip_table_size > 0) //when there is a blank line in file, token becomes NULL
      break;
    
    g_ip_table[g_ip_table_size].name = strdup(token);
   
    token = strtok(NULL, " \t\n");
    if(token == NULL)
    {
      perror("init_ip_table(): token is NULL");
      exit(1);
    }
    
    do
    {
      strcpy(ip_addr_buf, token);
      ptr = strstr(ip_addr_buf, "/");
      *ptr = 0;
      if(ptr+1 == NULL)
      {
	perror("init_ip_table(): there is no subnet mask information");
        exit(1);
      }

      strcpy(mask_buf, ptr+1);

      g_ip_table[g_ip_table_size].addr = inet_addr(ip_addr_buf); //IP address in network byte-order
      g_ip_table[g_ip_table_size].mask = make_subnet_mask(mask_buf); //subnet mask in network byte-order

#ifdef _DEBUG_
      printf("%s %s %s\n", g_ip_table[g_ip_table_size].name, ip_addr_buf, mask_buf); 
#endif
            
      token = strtok(NULL, " \t\n");
      if(token == NULL)
        break;

      g_ip_table_size++;
      g_ip_table[g_ip_table_size].name = strdup(g_ip_table[g_ip_table_size-1].name);

    } while(1); //end of do-while

    g_ip_table_size++;
  } //end of while

  return(1);
}

/* dump MAC table */
void dump_mac_table()
{
  int  i;
  char addr[32];
  
  for (i=0; i<g_mac_table_size; i++) {
    hwaddrtostr(g_mac_table[i].addr, addr);
    printf("%s %s\n", g_mac_table[i].name, addr);
  }
}

/* return MAC address given name */
int nametohwaddr(char *name, HwAddr addr)
{
  int  i;
  
  for (i=0; i<g_mac_table_size; i++) {
    if (strcmp(g_mac_table[i].name, name) == 0) {
      hwaddrcpy(addr, g_mac_table[i].addr);
      return(1);
    }
  }
  return(0);
}

/* return IP address given name */
int nametoipaddr(char *name, in_addr_t* addr)
{
  int  i;
  
  for (i=0; i<g_ip_table_size; i++) {
    if (strcmp(g_ip_table[i].name, name) == 0) {
      *addr = g_ip_table[i].addr;
      return(1);
    }
  }
  return(0);
}

/* return subnet mask given name */
int nametonetmask(char *name, int* mask)
{
  int  i;
  
  for (i=0; i<g_ip_table_size; i++) {
    if (strcmp(g_ip_table[i].name, name) == 0) {
      *mask = g_ip_table[i].mask;
      return(1);
    }
  }
  return(0);
}

/* DNS Lookup function to convert DNS name into IP addr */
int dns_name_to_ipaddr(char* dnsname, in_addr_t* ipaddr, int* ipaddr_num)
{
  int  i;

  *ipaddr_num = 0;
  for (i=0; i<g_ip_table_size; i++) {
    if (strcmp(g_ip_table[i].name, dnsname) == 0) {
      ipaddr[*ipaddr_num] = g_ip_table[i].addr;
      (*ipaddr_num)++;
    }
  }

  if(*ipaddr_num >= 1)
    return 1;
  else
    return 0;
}

/* get the netmasks for IP addresses */
int get_netmasks_for_addrs(in_addr_t* ipaddrs, int ipaddrs_num, int* netmasks)
{
  int i, j;
  int cnt = 0;
 
  for(i=0; i<ipaddrs_num; i++) //for-1
  {
    for(j=0; j<g_ip_table_size; j++) //for-2
    {
      if(g_ip_table[j].addr == ipaddrs[i])
      {
        netmasks[i] = g_ip_table[j].mask;
        cnt++;
        break;
      } //end of if
    } //end of for-2
  } //end of for-1

  return cnt;
}

/* return default gateway IP address given name */
int nametogwaddr(char *name, in_addr_t host_addr, int host_mask, in_addr_t* addr)
{
  int  i;
  in_addr_t ipaddr[ADDR_NUM];
  int ipaddr_num;
  int result;
  struct in_addr addr_struct;

  result = dns_name_to_ipaddr(name, ipaddr, &ipaddr_num);
  if(result == 0)
  {
    printf("nametogwaddr(): there is no entry for %s in DNS system\n", name);
    return 0;
  }
  
  for (i=0; i<ipaddr_num; i++) {
    if ((ipaddr[i] & host_mask) == (host_addr & host_mask)) {
      *addr = ipaddr[i];
      return 1;
    }
  }

  addr_struct.s_addr = host_addr;  
  printf("nametogwaddr(): there is no gw address for host address %s\n", inet_ntoa(addr_struct));
  return(0);
}

/* return name given hw addr */
int hwaddrtoname(HwAddr addr, char *name)
{
  int  i;

  for (i=0; i<g_mac_table_size; i++) {
    if (hwaddrcmp(g_mac_table[i].addr,addr) == 0) {
      strcpy(name, g_mac_table[i].name);
      return(1);
    }
  }
  return(0);
}

/* return name given IP addr */
int ipaddrtoname(in_addr_t addr, char *name)
{
  int  i;

  for (i=0; i<g_ip_table_size; i++) {
    if (g_ip_table[i].addr == addr) {
      strcpy(name, g_ip_table[i].name);
      return(1);
    }
  }
  return(0);
}

/*----------------------------------------------------------------*/
/* ARP function to convert IP addr with netmask into MAC addr */
int arp_ipaddr_to_hwaddr(in_addr_t ipaddr, HwAddr hwaddr)
{
  char name[NAME_SIZE];
  int result;
  struct in_addr addr;

  /* the MAC address of an IP packet broadcast in a LAN is the MAC broadcast address */
  if(ipaddr == IP_BCASTADDR)
  {
    memcpy(hwaddr, BCASTADDR, sizeof(HwAddr));
    return 1;
  } 

  addr.s_addr = ipaddr;

  result = ipaddrtoname(ipaddr, name);
  if(result == 0)
  {
    printf("arp_ipaddr_to_hwaddr(): there is no DNS name for %s\n", inet_ntoa(addr));
    return 0;
  }

  result = nametohwaddr(name, hwaddr);
  if(result == 0)
  {
    printf("arp_ipaddr_to_hwaddr(): there is no MAC address for %s\n", name);
    return 0;
  }

  return 1;
}

/*----------------------------------------------------------------*/


/*----------------------------------------------------------------*/
/* read n bytes from sd */
int readn(int sd, char *buf, int n)
{
  int     toberead;
  char *  ptr;

  toberead = n;
  ptr = buf;
  while (toberead > 0) {
    int byteread;

    byteread = read(sd, ptr, toberead);
    if (byteread <= 0) {
      if (byteread == -1)
	perror("read");
      return(0);
    }
    
    toberead -= byteread;
    ptr += byteread;
  }
  return(1);
}

/* recv an ether packet */
EthPkt *recvethpkt(int sd)
{
  EthPkt *ethpkt;
  
  /* allocate space for the ethpkt */
  ethpkt = (EthPkt *) calloc(1, sizeof(EthPkt));
  if (!ethpkt) {
    fprintf(stderr, "error : unable to calloc\n");
    exit(1);
  }

  /* read the header */
  if (!readn(sd, ethpkt->dst, sizeof(HwAddr))) {
    free(ethpkt);

    /** IMPORTANT CODE */
    set_hub_down(); //notify the application layer program that the hub associated with socket sd is down

    return(NULL);
  }
  if (!readn(sd, ethpkt->src, sizeof(HwAddr))) {
    free(ethpkt);
    return(NULL);
  }
  if (!readn(sd, (char *) &ethpkt->len, sizeof(ushort))) {
  //if (!readn(sd, (char *) &ethpkt->len, sizeof(short))) {
    free(ethpkt);
    return(NULL);
  }
  ethpkt->len = ntohs(ethpkt->len); //convert network byte-order into host byte-order

  /* allocate space for payload */
  ethpkt->dat = (char *) malloc(ethpkt->len);
  if (!ethpkt) {
    fprintf(stderr, "error : unable to malloc\n");
    exit(1);
  }

  /* read the data */
  if (!readn(sd, (char *) ethpkt->dat, ethpkt->len)) {
    freeethpkt(ethpkt);
    return(NULL);
  }

  /* done reading */
  return(ethpkt);
}

/* forward an ether packet in hub */
int forwardethpkt(int sd, EthPkt *ethpkt)
{
  int ret_val;

  ret_val = sendethpkt(sd, ethpkt);
  ethpkt->len = ntohs(ethpkt->len); //restore the length field of the ethernet packet into host-byte order for the correct forwarding of the same ethernet packet next time

  return ret_val;
} 

/* send an ether packet */
int sendethpkt(int sd, EthPkt *ethpkt)
{
  char * buf;
  char * ptr;
  ushort  ethpkt_length;
  ushort  len;
  int ret_val;

  /* allocate space for the buffer */
  len = 2*sizeof(HwAddr) + sizeof(ushort) + ethpkt->len;
  buf = (char *) calloc(len, sizeof(char));
  if (!buf) {
    fprintf(stderr, "error : unable to calloc\n");
    exit(1);
  }

  /* linearize the ethpkt. first head and then data */
  ptr = buf;
  memcpy(ptr, ethpkt->dst, sizeof(HwAddr));
  ptr += sizeof(HwAddr);

  memcpy(ptr, ethpkt->src, sizeof(HwAddr));
  ptr += sizeof(HwAddr);

  ethpkt_length = ethpkt->len; //use it later to copy the ethernet packet's payload into buf
  ethpkt->len = htons(ethpkt->len); //convert host byte-order into network byte-order
  memcpy(ptr, &(ethpkt->len), sizeof(ushort));
  ptr += sizeof(ushort);

  memcpy(ptr, (char *) ethpkt->dat, ethpkt_length);

  /* send the packet */
  ret_val = write(sd, buf, len);
  if (ret_val == -1) {
    perror("write() error!\n");
    free(buf);
    exit(1);
  }

  free(buf);
  return(1);
}

/* output packet contents */
void dumpethpkt(EthPkt *ethpkt)
{
  char bufr[MAXSTRING];
  
  /* common header */
  hwaddrtostr(ethpkt->dst, bufr);
  printf("Ethernet: %s", bufr);
  hwaddrtostr(ethpkt->src, bufr);
  printf(" | %s", bufr);
  printf(" | %d\n", ethpkt->len);
}

/* free up space allocated for ethpkt */
void freeethpkt(EthPkt *ethpkt)
{
  if(ethpkt->dat != NULL)
    free(ethpkt->dat);

  free(ethpkt);
}
/*----------------------------------------------------------------*/

/* send a message to IP stack */
int sendmessage(int sd, in_addr_t myaddr, in_addr_t dst, ushort len, u_char type, char* dat)
{
  IPPkt* ippkt; //IP packet
  struct in_addr addr;
  int ret_val;
      
  /* allocate space for the ippkt */
  ippkt = (IPPkt *) calloc(1, sizeof(IPPkt));
  if (!ippkt) {
    fprintf(stderr, "error : unable to calloc\n");
    return 0;
  }

  /* write the IP header */
  memcpy(&(ippkt->dst), &dst, sizeof(ippkt->dst));
  memcpy(&(ippkt->src), &myaddr, sizeof(ippkt->src));
  memcpy(&(ippkt->len), &len, sizeof(ippkt->len)); //host byte-order
  memcpy(&(ippkt->type), &type, sizeof(ippkt->type));

  /* allocate space for payload */
  ippkt->dat = (char *) malloc(len);
  if (!(ippkt->dat)) {
    fprintf(stderr, "error : unable to malloc\n");
    freeippkt(ippkt);
    return 0;

  }

  /* write the data */
  memcpy(ippkt->dat, dat, ippkt->len);  

  ret_val = sendippkt(sd, ippkt);
  if(ret_val != 1)
  {
    perror("sendippkt() sendippkt() error");
    freeippkt(ippkt);
    return 0;
  }
 
  freeippkt(ippkt);

  return 1;
}

/* send an application message, such as DV exchange message and chatting message */
int send_app_message(int sd, char* dst_name, ushort len, u_char type, char* dat)
{
  int ret_val;
  in_addr_t ipaddr[ADDR_NUM];
  int ipaddr_num = 0;
  in_addr_t dst; //destination IP address

  /* DNS Lookup function to convert DNS name into IP addr */
  if(type == DATA_CHAT)
  {
    if(dst_name == NULL)
    {
      printf("send_app_message(): the destination host's DNS name for chatting data should not be NULL.\n");
      return 0;
    }
    
    ret_val = dns_name_to_ipaddr(dst_name, ipaddr, &ipaddr_num);
    if(ret_val == 0)
    {
      printf("send_app_message(): the DNS name \"%s\" is not registered in our DNS system\n", dst_name);
      return 0;
    }

    dst = ipaddr[0];
  }
  else if(type == DATA_DV)
  {
    dst = IP_BCASTADDR; //the DV exchange message is broadcast to neighbor routers
  }
  else
  {
    printf("send_app_message(): the data type (%d) is not served!\n", type);
    return 0;
  }

  /** select an appropriate port with destination address (dst) */
  /** FILL IN YOUR CODE for dv_get_socket_for_destination() */
  if(g_station_kind == STATION_ROUTER)
    sd = dv_get_sock_for_destination(sd, g_myipaddrs[0], dst);
    //the selected source address of the router is the first IP address of the router, but we can enhance the source address selection.

  ret_val = sendmessage(sd, g_myipaddrs[0], dst, len, type, dat);
  if(ret_val != 1)
  {
    printf("send_app_message(): sendmessage() error!\n");
    return 0;
  }
  
  return 1;
}


/* recv a message from IP stack */
char* recvmessage(int sd, in_addr_t* src, ushort* len, u_char* type)
{
  IPPkt* ippkt; //IP packet
  struct in_addr addr;
  char* dat;
  int flag = 0; //it is used to know if there is an IP address matched with the destination IP address
  int i;
 
  ippkt = recvippkt(sd);
  if(ippkt == NULL) //indicate that the hub is down or that the received packet is not mine
    return NULL;

#ifdef _DEBUG
  dumpippkt(ippkt);
#endif

  for(i=0; i < g_myipaddrs_num; i++)
  {
    if(ippkt->dst == g_myipaddrs[i])
    {
      flag = 1;
      break;
    }
  }

  if((flag == 0) && (ippkt->dst != IP_BCASTADDR) && (g_station_kind == STATION_ROUTER))
  { /** FILL YOUR CODE: forward the data packet to next router or host according to the router's forwarding table */
    dv_forward(ippkt);
    return NULL;
  }
  else if((flag == 0) && (ippkt->dst != IP_BCASTADDR))
  {
    addr.s_addr = ippkt->dst;
    printf("recvmessage(): a wrongly destined IP packet with dst %s is received\n", inet_ntoa(addr));
    return NULL;  
  }


  *src = ippkt->src; //network byte-order
  *len = ippkt->len; //host byte-order
  *type = ippkt->type;

  /* allocate space to copy payload into */
  dat = (char *) malloc(ippkt->len);
  if (!dat) {
    fprintf(stderr, "error : unable to malloc\n");
    return NULL;
  }

  memcpy(dat, ippkt->dat, ippkt->len);

  freeippkt(ippkt);

  return dat;
}

/* recv an IP packet */
IPPkt *recvippkt(int sd)
{
  EthPkt *ethpkt; //Ethernet frame
  IPPkt *ippkt; //IP packet
  char* ptr;

  ethpkt = recvethpkt(sd);
  if(ethpkt == NULL) //indicate that the hub is down
    return NULL;

#ifdef _DEBUG
  dumpethpkt(ethpkt);
#endif

  if(hwaddrcmp(ethpkt->dst, BCASTADDR) != 0 && hwaddrcmp(ethpkt->dst, g_myhwaddr) != 0)
  {
    /* just ignore */
    printf("recvippkt(): a wrongly destined ethernet frame is received\n");
    //dumpethpkt(ethpkt);
    return NULL;
  }

  ptr = (char*) ethpkt->dat;
      
  /* allocate space for the ippkt */
  ippkt = (IPPkt *) calloc(1, sizeof(IPPkt));
  if (!ippkt) {
    fprintf(stderr, "error : unable to calloc\n");
    freeethpkt(ethpkt);
    exit(1);
  }

  /* read the IP header */
  memcpy(&(ippkt->dst), ptr, sizeof(ippkt->dst));
  ptr += sizeof(ippkt->dst);

  memcpy(&(ippkt->src), ptr, sizeof(ippkt->src));
  ptr += sizeof(ippkt->src);

  memcpy(&(ippkt->len), ptr, sizeof(ippkt->len));
  ptr += sizeof(ippkt->len);
  ippkt->len = ntohs(ippkt->len); //convert network byte-order into host byte-order

  memcpy(&(ippkt->type), ptr, sizeof(ippkt->type));
  ptr += sizeof(ippkt->type);

  /* allocate space for payload */
  ippkt->dat = (char *) malloc(ippkt->len);
  if (!(ippkt->dat)) {
    fprintf(stderr, "error : unable to malloc\n");
    freeethpkt(ethpkt);
    exit(1);
  }

  /* read the data */
  memcpy(ippkt->dat, ptr, ippkt->len);  

  freeethpkt(ethpkt);

  /* done reading */
  return(ippkt);
}

/* send an IP packet */
int sendippkt(int sd, IPPkt *ippkt)
{
  char * buf;
  char * ptr;
  ushort  len;
  ushort  ippkt_length;
  EthPkt *ethpkt; //Ethernet frame
  int ret_val;
  int i;
  int flag; //indicate the MAC address is already obtained

  /* allocate space for the buffer */
  len = 2*sizeof(in_addr_t) + sizeof(ushort) + sizeof(u_char) + ippkt->len;
  buf = (char *) calloc(len, sizeof(char));
  if (!buf) {
    fprintf(stderr, "error : unable to calloc\n");
    exit(1);
  }

  /* linearize the ethpkt. first head and then data */
  ptr = buf;

  memcpy(ptr, &(ippkt->dst), sizeof(in_addr_t));
  ptr += sizeof(in_addr_t);

  memcpy(ptr, &(ippkt->src), sizeof(in_addr_t));
  ptr += sizeof(in_addr_t);

  ippkt_length = ippkt->len; //use it later to copy the IP packet's payload into buf
  ippkt->len = htons(ippkt->len); //convert host byte-order into network byte-order
  memcpy(ptr, &(ippkt->len), sizeof(ushort));
  ptr += sizeof(ushort);

  memcpy(ptr, &(ippkt->type), sizeof(u_char));
  ptr += sizeof(u_char);

  memcpy(ptr, ippkt->dat, ippkt_length);

  /** make Ethernet packet */
  /* allocate space for the ethpkt */
  ethpkt = (EthPkt *) calloc(1, sizeof(EthPkt));
  if (!ethpkt) {
    fprintf(stderr, "error : unable to calloc\n");
    return 0;
  }

  /** find the src and dst MAC addresses through ARP function */
  arp_ipaddr_to_hwaddr(ippkt->src, ethpkt->src);
  
  /* the destination MAC address should be chosen according to the data type and the location of destination host */

  if(ippkt->type == DATA_DV)
    arp_ipaddr_to_hwaddr(IP_BCASTADDR, ethpkt->dst);
  else if(ippkt->type == DATA_CHAT) //else if-1
  {
    flag = 0;
    for(i = 0; i < g_mynetmasks_num; i++)
    {
      if((ippkt->src & g_mynetmasks[i]) == (ippkt->dst & g_mynetmasks[i])) //Since the destination host is located in the same network, the MAC address of the destination host is used.
      //if((g_myipaddr & g_mynetmask) == (ippkt->dst & g_mynetmask)) //Since the destination host is located in the same network, the MAC address of the destination host is used.
      {
        arp_ipaddr_to_hwaddr(ippkt->dst, ethpkt->dst);
        flag = 1; //indicate the MAC address is already obtained
        break;
      }
    } //end of for

    if(flag != 1) //if-2
    {
      if(g_station_kind == STATION_HOST) //the packet should be sent to the default router, the MAC address of the default router is used.
        arp_ipaddr_to_hwaddr(g_mygwaddr, ethpkt->dst);
      else if(g_station_kind == STATION_ROUTER) 
	/** FILL IN YOUR CODE for dv_ipaddr_to_hwaddr() */ 
        dv_ipaddr_to_hwaddr(ippkt->dst, ethpkt->dst); //convert the dst IP address into next hop's MAC address
      else
      {
        printf("sendippkt(): g_station_kind (%d) is not supported to send IP packet\n", g_station_kind);
        return 0;
      }
    } //end of if-2
  } //end of else if-1
  else
  {
    printf("sendippkt(): Unknow data type (%d)!\n", ippkt->type);
    return 0;
  }
  
  memcpy(&(ethpkt->len), &len, sizeof(ethpkt->len)); //host byte-order

  /* allocate space for payload */
  ethpkt->dat = (char *) malloc(ethpkt->len);
  if (!(ethpkt->dat)) {
    fprintf(stderr, "error : unable to malloc\n");
    free(ethpkt);
    return 0;
  }

  /* write the data */
  memcpy(ethpkt->dat, buf, len); 

  /* send the packet to MAC layer */
  ret_val = sendethpkt(sd, ethpkt);
  if (ret_val == -1) {
    perror("sendippkt(): write() error!\n");
    free(ethpkt);
    return 0;
  }
  
  freeethpkt(ethpkt); //freeethpkt() frees the memory of ethpkt and ethpkt->dat
  free(buf); //free the memory allocated for the ip packet
  return(1);
}

/* output IP packet contents */
void dumpippkt(IPPkt *ippkt)
{
  char bufr[MAXSTRING];
  struct in_addr addr;
  
  /* common header */
  addr.s_addr = ippkt->dst;
  printf("IP: %s", inet_ntoa(addr));
  addr.s_addr = ippkt->src;
  printf(" | %s", inet_ntoa(addr));
  printf(" | %d", ippkt->len);
  printf(" | %d\n", ippkt->type);

}

/* free up space allocated for ippkt */
void freeippkt(IPPkt *ippkt)
{
  if(ippkt->dat != NULL)
    free(ippkt->dat);

  free(ippkt);
}


/*----------------------------------------------------------------*/
/* convert string to hardware address */
int strtohwaddr(char *str, HwAddr adr)
{
  int  byte;
  char *ptr;
  int  i;
  
  ptr = str;
  for (i=0; i<5; i++) {
    char *tmp;

    if (sscanf(ptr, "%x", &byte) < 1)
      return(0);
    adr[i] = byte;
    
    tmp = index(ptr, ':');
    /* index(const char* s, int c): return a pointer to the first occurrence of character c in string s. */

    if (!tmp)
      return(0);
    ptr = tmp+1;
  }
  if (sscanf(ptr, "%x", &byte) < 1)
    return(0);
  adr[i] = byte;

  return(1);
}

/* convert hardware address to string */
int hwaddrtostr(HwAddr adr, char *str)
{
  int j, k;
  const char hexbuf[] =  "0123456789ABCDEF";
  
  for(j=0, k=0; j<6;j++) {
    str[k++]=hexbuf[ (adr[j]>>4)&15 ];
    str[k++]=hexbuf[  adr[j]&15     ];
    str[k++]=':';
  }
  str[--k]=0;

  return(1);
}

/* compare two hardware addresses */
int hwaddrcmp(HwAddr adr1, HwAddr adr2)
{
  return(memcmp(adr1, adr2, sizeof(HwAddr)));
}

/* copy hard address from adr2 to adr1 */
int hwaddrcpy(HwAddr adr1, HwAddr adr2)
{
  memcpy(adr1, adr2, sizeof(HwAddr));
}
/*----------------------------------------------------------------*/


/*----------------------------------------------------------------*/
/* hub calls this to start the lan */
int initlan(char *lan)
{
  int                sd;
  struct sockaddr_in myaddr;
  int addrlen;

  char myhostname[MAXSTRING];
  struct hostent *myhostent;

  char linktrgt[MAXSTRING];
  char linkname[MAXSTRING];

  /* create a socket */
  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    perror("socket");
    return(-1);
  }

  /* bind the socket to some port */
  bzero((char *) &myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(0);
  if (bind(sd, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
    perror("bind");
    return(-1);
  }
  listen(sd, 5);
  
  /* figure out the port of self */
  addrlen = sizeof(myaddr);
  if (getsockname(sd, (struct sockaddr *) &myaddr, &addrlen) == -1) {
    perror("getsockname");
    return(-1);
  }

  /* figure out the name of self */
  gethostname(myhostname, MAXSTRING);
  myhostent = gethostbyname(myhostname);
  if (!myhostent) {
    perror("gethostbyname");
    return(-1);
  }

  /* create a link to let others know about self */
  sprintf(linktrgt, "%s:%d", myhostent->h_name,
	  (int) ntohs(myaddr.sin_port));
  sprintf(linkname, ".%s.info", lan);
  if (symlink(linktrgt, linkname) != 0) {
    /* symlink(const char* name1, const char* name2): create a symbolic link name2 to the file name1. */ 
    fprintf(stderr, "error : hub already exists\n");
    return(-1);
  }

  /* ready to accept requests */
  printf("admin: started hub on '%s' at '%d'\n",
	 myhostent->h_name, (int) ntohs(myaddr.sin_port));
  return(sd);
}

/*----------------------------------------------------------------*/
/* /\* DNS server calls this to make symbolic link to contain its DNS name and port *\/ */
/* int initdns(char *dns) */
/* { */
/*   int                sd; */
/*   struct sockaddr_in myaddr; */
/*   int addrlen; */

/*   char myhostname[MAXSTRING]; */
/*   struct hostent *myhostent; */

/*   char linktrgt[MAXSTRING]; */
/*   char linkname[MAXSTRING]; */

/*   /\* create a socket *\/ */
/*   sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); */
/*   if (sd == -1) { */
/*     perror("socket"); */
/*     return(-1); */
/*   } */

/*   /\* bind the socket to some port *\/ */
/*   bzero((char *) &myaddr, sizeof(myaddr)); */
/*   myaddr.sin_family = AF_INET; */
/*   myaddr.sin_addr.s_addr = htonl(INADDR_ANY); */
/*   myaddr.sin_port = htons(0); */
/*   if (bind(sd, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) { */
/*     perror("bind"); */
/*     return(-1); */
/*   } */
/*   listen(sd, 5); */
  
/*   /\* figure out the port of self *\/ */
/*   addrlen = sizeof(myaddr); */
/*   if (getsockname(sd, (struct sockaddr *) &myaddr, &addrlen) == -1) { */
/*     perror("getsockname"); */
/*     return(-1); */
/*   } */

/*   /\* figure out the name of self *\/ */
/*   gethostname(myhostname, MAXSTRING); */
/*   myhostent = gethostbyname(myhostname); */
/*   if (!myhostent) { */
/*     perror("gethostbyname"); */
/*     return(-1); */
/*   } */

/*   /\* create a link to let others know about self *\/ */
/*   sprintf(linktrgt, "%s:%d", myhostent->h_name, */
/* 	  (int) ntohs(myaddr.sin_port)); */
/*   sprintf(linkname, ".%s.info", dns); */
/*   if (symlink(linktrgt, linkname) != 0) { */
/*     /\* symlink(const char* name1, const char* name2): create a symbolic link name2 to the file name1. *\/  */
/*     fprintf(stderr, "error : DNS server already exists\n"); */
/*     return(-1); */
/*   } */

/*   /\* ready to accept requests *\/ */
/*   printf("admin: started DNS server on '%s' at '%d'\n", */
/* 	 myhostent->h_name, (int) ntohs(myaddr.sin_port)); */
/*   return(sd); */
/* } */

/* stations call this to connect to hub */
int hooktolan(char *lan)
{
  int sd;

  struct sockaddr_in saddr;
  struct hostent *he;
  
  char linkname[MAXSTRING];
  char linktrgt[MAXSTRING];
  char *servhost, *servport;
  int  bytecnt;
  
  /* locate server */
  sprintf(linkname, ".%s.info", lan);
  bytecnt = readlink(linkname, linktrgt, MAXSTRING);
  /* readlink(): read the contents of a symbolic link */

  if (bytecnt == -1) {
    fprintf(stderr, "error : no active hub on '%s'\n", lan);
    return(-1);
  }
  linktrgt[bytecnt] = '\0';

  /* split addr into host and port */
  servport = index(linktrgt, ':');
  *servport = '\0';
  servport++;
  servhost = linktrgt;

  /* create a socket */
  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd == -1) {
    perror("socket");
    return(-1);
  }

  /* form the server's address */
  bzero((char *) &saddr, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(atoi(servport));
  he = gethostbyname(servhost);
  if (!he) {
    perror(servhost);
    return(-1);
  }
  bcopy(he->h_addr, (char *) &saddr.sin_addr, he->h_length);

  /* get connnected to the server */
  if (connect(sd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
    perror("connect");
    return(-1);
  }
  
  /* succesful. return socket descriptor */
  printf("admin: connected to hub on '%s' at '%s'\n",
	 servhost, servport);
  return(sd);
}

/* get the current time in secs (since 1970) */
long getcurtime()
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return(tv.tv_sec);
}

/* convert secs to hour:min:sec format */
char *timetostring(long secs)
{
  struct tm *    tm;
  static char    curtime[32];
  tm = localtime(&secs);
  sprintf(curtime, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
  return(curtime);
}

/* return the string for the current time information */
char* getcurtimeinfo()
{
  return timetostring(getcurtime());
}
/*----------------------------------------------------------------*/
