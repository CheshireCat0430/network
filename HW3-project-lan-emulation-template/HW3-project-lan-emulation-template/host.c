/*--------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include "common.h"

/* my DNS name */
char myname[NAME_SIZE];

/* my MAC address */
HwAddr myhwaddr;

/* my IP address in network byte-order */

in_addr_t myipaddr;

/* my subnet mask in network byte-order */
int mynetmask;

/* my default gateway */
in_addr_t mygwaddr;

/* socket to hub */
int sd = -1;
/*--------------------------------------------------------------------*/

void print_menu()
{
  printf("#############################################\n");
  printf("hostname message : send a message to the host\n");
  printf("help             : print the menu\n");
  printf("#############################################\n");
  fflush(NULL);
}

/*--------------------------------------------------------------------*/
/* process packet data */
int processdata(char* dat, int len, int type, in_addr_t src_addr)
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
  send_app_message(sd, destname, len, type, text);

  return(1);
}

/*--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  /* check usage */
  if (argc != 4) {
    printf("usage : %s <my-name> <lan-name> <default-gateway>\n", argv[0]);
    exit(1);
  }

  /* print menu */
  print_menu();

  /* set station kind */
  set_station_kind(STATION_HOST);

  /* get hooked on to the lan */
  if((sd = hooktolan(argv[2])) == -1)
    exit(1);

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

  /* note my own IP address */
  if (!nametoipaddr(argv[1], &myipaddr)) {
    fprintf(stderr, "error : unable to identify my IP address\n");
    exit(1);
  }

  /* note my own subnet mask */
  if (!nametonetmask(argv[1], &mynetmask)) {
    fprintf(stderr, "error : unable to identify my subnet mask\n");
    exit(1);
  }
  
  /* note my own default gateway IP address */
  if (!nametogwaddr(argv[3], myipaddr, mynetmask, &mygwaddr)) {
    fprintf(stderr, "error : unable to identify my default gateway address\n");
    exit(1);
  }

  /* register my address information with g_myhwaddr and g_myipaddrs for checking if the received packet is mine or not and for sending my packet to the specified destination */
  set_host_addrinfo(myhwaddr, myipaddr, mynetmask, mygwaddr);

  /* keep moving packets around */
  while (1) {
    fd_set readset;

    /* watch stdin and socket */
    FD_ZERO(&readset);
    FD_SET(0,  &readset);
    
    if(sd != -1)
      FD_SET(sd, &readset);

    if (select(sd+1, &readset, NULL, NULL, NULL) == -1) {
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
      if(strcasecmp(bufr, "help") == 0)
	print_menu();
      else
        processtext(bufr);
    }

    /* something from the hub? */
    if (FD_ISSET(sd, &readset)) {
      in_addr_t src_addr; //source IP address of the received packet
      ushort len; //data length //@ it should be "ushort", not "int" in order to match with "len" field in IP header 

      char* dat = NULL; //payload of the received packet
      u_char type; //data type = {DATA_DV, DATA_CHAT}

      set_hub_up(); //set the hub related to socket sd to HUB_UP. After calling recvmessage(), if hub_status() returns HUB_DOWN, it means that the hub related to socket sd is down. So we need to close sd.      

      dat = (char*) recvmessage(sd, &src_addr, &len, &type);
      /* NOTE: the compiler complains if there is no type casting like above */

      if (dat == NULL && (hub_status() == HUB_DOWN)) {
        fprintf(stderr, "error: hub for '%s' is down\n", argv[2]);
        close(sd);
        sd = -1;
      }
      else if(dat != NULL) //if the received packet is mine, process it.
        processdata(dat, len, type, src_addr);
    }
  }
}
/*--------------------------------------------------------------------*/
