/* 
 * Encapsulate the logic associated with downloading files from peers
 */

/*************************** U N T E S T E D ***************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "bitclient.h"
#include "leecher.h"

/* 
 * Bind to a TCP socket. We'll use it to get chunks from peers
 */
int
get_sock_fd(char *port)
{
    int fd;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* IP version agnostic */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_flags = AI_PASSIVE;     /* Bind to localhost's IP */

    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        /* Get a socket */
        if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            perror("socket");
            return -1;
        }

        /* Prevent "bind: Address already in use" error */
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
            perror("setsockopt");
            return -1;
        }

        /* Bind the socket to a port */
        if (bind(fd, p->ai_addr, p->ai_addrlen) < 0) {
            perror("bind");
            return -1;
        }

        break;
    }

    if (p == NULL) {
        FATAL("There weren't any sockets for leecher to bind too :(\n");
        return -1;
    }

    freeaddrinfo(servinfo);
    return fd;
}

void *
leecher_tmain(void *raw)
{
    torrent_t *t = (torrent_t*)raw;
    int file_fd = -1, sock_fd = -1;

    if (t == NULL) return NULL;

    /* I *think* O_SYNC will cause it to be written out to disk with each 
     * write(2) call so we don't keep the whole thing in memory. 
     * TODO: Thread safety? */
    /* if ((file_fd = open(t->filename, O_CREAT | O_APPEND | O_SYNC)) < 0) { */
    /*     perror("open"); */
    /*     return NULL; */
    /* } */

    /* Establish a TCP socket */
    if ((sock_fd = get_sock_fd(t->port)) < 0) {
        FATAL("Leecher failed to establish a socket\n");
        return NULL;
    }

    /* Loop through the segments, we'll fetch them sequentially for simplicity */
    /* for (chunk_t *c = t->pieces; c != NULL; c = c->next) { */

    /* } */

    /* Once the file has been fully downloaded, inform the user and exit */

    return (void*)(size_t)file_fd;      /* file_fd shouldn't be 0 */
}
