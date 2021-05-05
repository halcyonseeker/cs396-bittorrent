/* 
 * Encapsulate the logic associated with downloading files from peers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "bitclient.h"
#include "leecher.h"

void *
leecher_tmain(void *raw)
{
    torrent_t *t = (torrent_t*)raw;
    /* int file_fd, sock_fd; */

    if (t == NULL) return NULL;

    /* Open the specified file in append mode */

    /* Establish a TCP socket */

    /* Loop through the segments, we'll get the sequentially for simplicity */

    /*     Loop through the peers */
    /*         If it times out; continue */
    /*         If it sends a broken chunk; re-request it */
    /*         If it sends a valid chunk; break */

    /*     Append the chunk to file_fd and write it do the disk */

    /* Once the file has been fully downloaded, inform the user and exit */

    return NULL;
}
