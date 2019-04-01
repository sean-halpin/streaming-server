/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gst/gst.h>

#define BUFSIZE 1024

#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char **argv)
{
    int parentfd;                  /* parent socket */
    int childfd;                   /* child socket */
    int portno;                    /* port to listen on */
    int clientlen;                 /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char buf[BUFSIZE];             /* message buffer */
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */

    /* 
   * check command line arguments 
   */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* 
   * socket: create the parent socket 
   */
    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parentfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    /*
   * build the server's Internet address
   */
    bzero((char *)&serveraddr, sizeof(serveraddr));

    /* this is an Internet address */
    serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
   * bind: associate the parent socket with a port 
   */
    if (bind(parentfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /* 
   * listen: make this socket ready to accept connection requests 
   */
    if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */
        error("ERROR on listen");

    /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
    clientlen = sizeof(clientaddr);
    while (1)
    {

        /* 
     * accept: wait for a connection request 
     */
        childfd = accept(parentfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (childfd < 0)
            error("ERROR on accept");

        /* 
     * gethostbyaddr: determine who sent the message 
     */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server established connection with %s (%s)\n",
               hostp->h_name, hostaddrp);

        /* 
     * read: read input string from the client
     */
        bzero(buf, BUFSIZE);
        n = read(childfd, buf, BUFSIZE);
        if (n < 0)
            error("ERROR reading from socket");
        printf("server received %d bytes: %s", n, buf);

        int i = 0;
        char *p = strtok(buf, " ");
        char *array[10];

        while (p != NULL)
        {
            array[i++] = p;
            p = strtok(NULL, " ");
        }

        /*
     * Start GStreamer Pipeline
     */
        GstElement *pipeline;
        GstBus *bus;
        GstMessage *msg;

        gst_println("RTP Session Started");
        /* Print Args */
        gst_println("pattern: %s", array[1]);
        gst_println("remote host: %s", array[2]);
        gst_println("client rtp port: %s", array[3]);
        gst_println("client rtcp port: %s", array[4]);
        gst_println("server rtcp port: %s", array[5]);

        if (strcmp(array[0], "play") == 0)
        {

            /* Initialize GStreamer */
            gst_init(&argc, &argv);

            /* Build Pipeline String */
            gchar buffer[1024];
            g_snprintf(buffer, sizeof(buffer),
                       "rtpbin name=rtpbin autoremove=true "
                       "videotestsrc pattern=%s ! videoconvert ! x264enc ! rtph264pay ! rtpbin.send_rtp_sink_0 "
                       "rtpbin.send_rtp_src_0 ! udpsink name=rtpudpsink host=%s port=%s "
                       "rtpbin.send_rtcp_src_0 ! udpsink name=rtcpudpsink  host=%s port=%s sync=false async=false "
                       "udpsrc name=rtcpudpsrc port=%s ! rtpbin.recv_rtcp_sink_0",
                       array[1], array[2], array[3], array[2], array[4], array[5]);
            gst_println(buffer);
            /* Build the pipeline */
            pipeline = gst_parse_launch(buffer, NULL);

            /* Start playing */
            gst_element_set_state(pipeline, GST_STATE_PLAYING);

            /* Wait until error or EOS */
            bus = gst_element_get_bus(pipeline);
            msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

            /* Free resources */
            if (msg != NULL)
                gst_message_unref(msg);
            gst_object_unref(bus);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }

        /* 
     * write: echo the input string back to the client 
     */
        n = write(childfd, buf, strlen(buf));
        if (n < 0)
            error("ERROR writing to socket");

        close(childfd);
    }
}