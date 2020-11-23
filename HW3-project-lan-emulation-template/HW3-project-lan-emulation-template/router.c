/*--------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <ctype.h> //isdigit()
#include "common.h"
#include <signal.h> //signal()
#include <errno.h> //errno

/* my DNS name */
char myname[NAME_SIZE];

/* my MAC address */
HwAddr myhwaddr;

/* my IP addresses in network byte-order */
in_addr_t myipaddrs[ADDR_NUM];
int myipaddrs_num;

/* my subnet mask in network byte-order */
int mynetmasks[ADDR_NUM];
int mynetmasks_num;

/* my configint which is the time interval for DV message advertisement */
int myconfigint;

/* sockets to hubs */
int sds[ADDR_NUM];
int sds_num;

/*--------------------------------------------------------------------*/

void print_menu()
{
  printf("#############################################\n");
  printf("show rt          : show routing table\n");
  printf("show ft          : show forwarding table\n");
  printf("hostname message : send a message to the host\n");
  printf("help             : print the menu\n");
  printf("#############################################\n");
  fflush(NULL);
}
/*--------------------------------------------------------------------*/
/* process packet data */
int processdata(int sock, char* dat, int len, int type, in_addr_t src_addr)
{

  if(type == DATA_CHAT)
  {
    char name[NAME_SIZE];

    /* for us. display contents */
    ipaddrtoname(src_addr, name);
    dat[len] = '\0';

    printf("%s : %s\n", name, dat);
    fflush(NULL);

   /** the memory should be freed */
    free(dat);
  }
  else if(type == DATA_DV)
  { /** FILL IN YOUR CODE in dv_update_routing_info() function */
    dv_update_routing_info(sock, dat, len, src_addr);

   /** the memory should be freed */
    free(dat);
  }
  
  return(1);
}

/* process the keyboard input */
int processtext(char *text)
{
  char*  destname;
  int type; //data type
  int len; //data length

  /* figure out the dest host */
  destname = text;
  text = index(text, ' ');
  if (text == NULL) {
    fprintf(stderr, "error: message undecipherable\n");
    return(0);
  }
  *text = '\0';
  text++;

  /* prepare for IP packet header information */
  len = strlen(text);
  type = DATA_CHAT;

  /* send user data to IP layer */
  send_app_message(sds[0], destname, len, type, text);

  return(1);
}

/* set the timer, generates a SIGALRM signal */
void setalarm(int interval)
{
  struct itimerval timer;

  timer.it_interval.tv_usec = 0;
  timer.it_interval.tv_sec  = interval;
  timer.it_value.tv_usec    = 0;
  timer.it_value.tv_sec     = interval;
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
    perror("setitimer");
    exit(1);
  }
}

/* handler for timer (config or reconfig) expiration */
void timeout()
{
  long curtime;
  int ret_val;

  /* note current time */
  curtime = getcurtime();
#ifdef _DEBUG
  printf("timeout(): current time: %s\n", timetostring(curtime));
#endif
  
  /* re-set the signal handler */
  signal(SIGALRM, timeout);

  /** FILL IN YOUR CODE in dv_update_tables_for_timeout() function */
  dv_update_tables_for_timeout(curtime, myconfigint);
 
  /* broadcast its routing information through DV message */
  ret_val = dv_broadcast_dv_message();
  if(ret_val != 1)
  {
    printf("timeout(): the router cannot broadcast its routing information to its neighbors\n");
    return;
  }
}

/*--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  int ret_val;
  int i, j;
  int hubsock; //socket for a hub
  int max_sd; //maximum socket descriptor used in select()
  int len;
  char buf[BUF_SIZE2];

  /* check usage */
  if (argc < 4) {
    printf("usage : %s <my-name> <configint> <lan-name-1> [<lan-name-2> ... ]\n", argv[0]);
    exit(1);
  }

  /* print menu */
  print_menu();

  sds_num = 0;
  max_sd = 0;
  /* get hooked on to the lans */
  for(i = 0; i < argc-3; i++)
  {
    if((sds[i] = hooktolan(argv[3+i])) == -1)
      exit(1);

    add_lanname_entry(sds[i], argv[3+i]);
 
    if(sds[i] > max_sd)
      max_sd = sds[i];
    
    sds_num++;
  }

  /* set station kind */
  set_station_kind(STATION_ROUTER);

  /* set up configint for DV message exchange */
  len = strlen(argv[2]);
  strcpy(buf, argv[2]);

  for(i = 0; i < len; i++)
  {
    if(isdigit(buf[i]) == 0)
    {
      printf("argv[2] is invalid:configint should be nonnegative integer!\n");
      exit(1);
    }
  }
  
  myconfigint = atoi(argv[2]);
  if((myconfigint < 0) || (myconfigint > MAX_CONFIG_INTERVAL))
  {
    printf("myconfigint (%d) is not valid since it is not [%d, %d]\n", myconfigint, 0, MAX_CONFIG_INTERVAL);
    exit(1);
  }
  
  /* init mac address configuration table */
  if (!init_mac_table(MAC_FILE))
    exit(1);

  /* init IP address configuration table */
  if (!init_ip_table(IP_FILE))
    exit(1);

  /* init default gateway configuration table */
  if (!init_gw_table(GW_FILE))
    exit(1);

  /* note my DNS name */
  strcpy(myname, argv[1]);

  /* note my own MAC address */
  if (!nametohwaddr(argv[1], myhwaddr)) {
    fprintf(stderr, "error : unable to identify my MAC address\n");
    exit(1);
  }

  /* note my own IP addresses */
  if(!dns_name_to_ipaddr(myname, myipaddrs, &myipaddrs_num))
  {
    printf("there is no entry for %s in DNS system\n", myname);
    exit(1);
  }

  /* note my own subnet masks */
  ret_val = get_netmasks_for_addrs(myipaddrs, myipaddrs_num, mynetmasks);

  if (ret_val != myipaddrs_num)
  {
    fprintf(stderr, "error : unable to identify all my subnet masks\n");
    exit(1);
  }
  
  /* register my address information with g_myhwaddr and g_myipaddr for checking if the received packet is mine or not and for sending my packet to the specified destination */
  set_router_addrinfo(myhwaddr, myipaddrs, myipaddrs_num, mynetmasks);

  /* determine whether to run DV routing protocol or not according to myconfigint */
  if(myconfigint > 0)
  {
    /* set timer to generate DV message */
    setalarm(myconfigint);

    /* mimic a timeout to send DV msg now */
    timeout();
  }

  /* keep moving packets around */
  while (1) { //while
    fd_set readset;

    /* watch stdin and socket */
    FD_ZERO(&readset);
    FD_SET(0, &readset);

    if(sds_num == 0)
    {
      printf("There is no active hub connected to this router!\n");
      return 0;
    }
    
    for(i = 0; i < sds_num; i++)
    { 
      if(sds[i] != -1)
        FD_SET(sds[i], &readset);
    }

    if (select(max_sd+1, &readset, NULL, NULL, NULL) == -1)
    {
      if(errno == EINTR) //Interrupted system call by SIGALRM
        continue;

      perror("select");
      exit(1);
    }

    /* any keyboard input? */
    if (FD_ISSET(0, &readset)) {
      char bufr[MAXSTRING];
      ushort len; //data length //@ it should be "ushort", not "int" in order to match with "len" field in IP header 
 
      fgets(bufr, MAXSTRING, stdin); //the string returned by fgets() has '\n' and so we remove it.
      len = strlen(bufr);
      bufr[len-1] = '\0';

      /** FILL IN YOUR CODE: show routing table and forwarding table */
      if(strcasecmp(bufr, "show rt") == 0)
        dv_show_routing_table();
      else if(strcasecmp(bufr, "show ft") == 0)
        dv_show_forwarding_table();
      else if(strcasecmp(bufr, "help") == 0)
	print_menu();
      else
        processtext(bufr);
    }

    /* something from the hub? */
    for(i = 0; i < sds_num; i++) //for-1
    {
      hubsock = sds[i];

      if (FD_ISSET(hubsock, &readset)) {//if-1
        in_addr_t src_addr; //source IP address of the received packet
        ushort len; //data length //@ it should be "ushort", not "int" in order to match with "len" field in IP header 
        
        char* dat = NULL; //payload of the received packet
        u_char type; //data type = {DATA_DV, DATA_CHAT}

        set_hub_up(); //set the hub related to socket sd to HUB_UP. After calling recvmessage(), if hub_status() returns HUB_DOWN, it means that the hub related to socket sd is down. So we need to close sd.      

        dat = (char*) recvmessage(hubsock, &src_addr, &len, &type);
        /* NOTE: the compiler complains if there is no type casting like above */

        if (dat == NULL && (hub_status() == HUB_DOWN)) {
          char* lanname;
          lanname = (char*) get_lanname(hubsock);
          if(lanname !=NULL)
	  {
            fprintf(stderr, "the hub for '%s' is down\n", lanname);
            delete_lanname_entry(hubsock);

            /** FILL IN YOUR CODE in dv_broadcast_dv_message_for_link_breakage() function */

            dv_broadcast_dv_message_for_link_breakage(hubsock); //broadcast the routing information with DV exchange message containing the network address related to the link which is broken due to a hub crash.
          }

          close(hubsock);

          /* adjust sds[], sds_num and max_sd */
          for(j = 0; j < sds_num; j++)
          {
            if(hubsock == sds[j])
            {
              sds[j] = sds[sds_num - 1];
              sds_num--; 
              i = j - 1; //adjust index i since i increases by one the end of the loop, but this socket corresponding to index i should be checked next time
              break;
            } //end of if
          } //end of for

          /* find out max_sd that is the greatest number */
          if(hubsock == max_sd)
          {
            for(j = 0, max_sd = sds[0]; j < sds_num; j++)
            {
              if(max_sd < sds[j]);
                max_sd = sds[j];
            } //end of for
          } //end of if
        }
        else if(dat != NULL)
          processdata(hubsock, dat, len, type, src_addr);
      } //end of if-1
    } //end of for-1

  } //end of while
}
/*--------------------------------------------------------------------*/
