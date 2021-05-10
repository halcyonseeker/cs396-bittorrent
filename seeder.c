/* 
 * Encapsulate the logic associated with uploading chunks to peers
 */


/*************************** U N T E S T E D ***************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "bitclient.h"
#include "seeder.h"

void *
seeder_tmain(void *raw)
{
    torrent_t *t = (torrent_t*)raw;

    if (t == NULL) return NULL;

    /* Establish a TCP socket */

    /* Register a signal handler to die gracefully and inform the trackers */

    /* Start an infinte loop listening on t->port */
    /*     Check what we've got and inform the tracker */
    /*     If a peer requests a chunk, read it from disk and send it */

    return NULL;
}
