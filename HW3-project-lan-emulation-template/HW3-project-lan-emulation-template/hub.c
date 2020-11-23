/*--------------------------------------------------------------------*/
/* hub.c */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <time.h> 
#include <errno.h>
#include <signal.h>

#include "common.h"
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/

char *mylan;

/* clean up before exit */
void cleanup()
{
  char linkname[MAXSTRING];

  /* unlink the link */
  sprintf(linkname, ".%s.info", mylan);
  unlink(linkname);
    
  exit(0);
}

/* main routine */
main(int argc, char *argv[])
{
  struct in_addr subnet; //subnet address
  int subnet_mask; //subnet mask

  int    servsock;
  fd_set livesdset;
  int    livesdmax;

  /* check usage */
  if (argc != 2) {
    fprintf(stderr, "usage : %s <my lan name>\n", argv[0]);
    exit(1);
  }

  /* set station kind */
  set_station_kind(STATION_HUB);

  mylan = strdup(argv[1]);

  /* setup signal handlers to clean up */
  signal(SIGTERM, cleanup);
  signal(SIGINT, cleanup);

  /* get ready to receive requests */
  servsock = initlan(mylan);
  if (servsock == -1) {
    exit(1);
  }

  /* form a set of active descriptors */
  FD_ZERO(&livesdset);
  FD_SET(servsock, &livesdset);
  FD_SET(0, &livesdset);

  livesdmax = servsock;

  /* accept requests and process them */
  while (1) {
    int    frsock, tosock;
    fd_set readset;

    /* wait for requests */
    memcpy(&readset, &livesdset, sizeof(livesdset));
    if (select(livesdmax+1, &readset, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(1);
    }

    /* figure out the request and serve it */
    for (frsock=3; frsock <= livesdmax; frsock++) {

      /* skip the server socket */
      if (frsock == servsock) continue;

      /* poll existing clients */
      if (FD_ISSET(frsock, &readset)) {
	EthPkt *pkt;

	/* read the message */
	pkt = recvethpkt(frsock); //pkt->len is host-byte order after calling recvethpkt().

	if (!pkt) {
	  struct sockaddr_in caddr;
	  int                caddrlen;
	  struct hostent *   cent;

	  /* disconnect from client */
	  caddrlen = sizeof(caddr);
	  if (getpeername(frsock, (struct sockaddr *) &caddr, &caddrlen) == -1) {
	    perror("getpeername");
	  }
	  cent = gethostbyaddr((char *) &caddr.sin_addr,
			       sizeof(caddr.sin_addr), AF_INET);
	  printf("admin: disconnect from '%s' at '%d'\n",
		 cent->h_name, frsock);

	  /* no more watching this sock */
	  close(frsock);
	  FD_CLR(frsock, &livesdset);

	} else {
	  /* send the pkt to all others */
	  for (tosock=3; tosock <= livesdmax; tosock++) {
	    /* skip server socket and sender socket */
	    if (tosock == servsock || tosock == frsock) continue;

	    if (FD_ISSET(tosock, &livesdset))
		forwardethpkt(tosock, pkt);
	  }

#ifdef _DEBUG
	  printf("sent --> ");
	  //pkt->len = ntohs(pkt->len); //convert network byte-order into host byte-order
	  dumpethpkt(pkt);
#endif

	  /* free the pkt */
	  freeethpkt(pkt);
	}
      }
    }

    /* look for connects from new clients */
    if (FD_ISSET(servsock, &readset)) {
      struct sockaddr_in caddr;
      int                caddrlen;
      int                csd;

      /* accept a connection request */
      caddrlen = sizeof(caddr);
      csd = accept(servsock, (struct sockaddr *) &caddr, &caddrlen);
      if (csd != -1) {
	struct hostent *cent;
		
	/* include this in the active sd set */
	FD_SET(csd, &livesdset);
	if (csd > livesdmax)
	  livesdmax = csd;

	/* figure out the client */
	cent = gethostbyaddr((char *) &caddr.sin_addr,
			   sizeof(caddr.sin_addr), AF_INET);
	printf("admin: connect from '%s' at '%d'\n",
	       cent->h_name, csd);
      } else {
	perror("accept");
	exit(0);
      }
    }
  }
}
/*--------------------------------------------------------------------*/
