/*
 * main.c: A simple BitTorrent client
 * Thalia Wright <wrightng@reed.edu>
 * DUE MAY 10
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <curl/curl.h>

#include "bitclient.h"
#include "magnet.h"

int log_verbosely = 0;

#define USAGE                                                                  \
    "\
Usage: bitclient [-vh] magnet:\n\
    Options:\n\
        -v || --verbose        Log debugging information\n\
        -h || --help           Print this message and exit\n"

void
print_torrent(torrent_t *t)
{
    printf("Torrent structure:\n");
    printf("\tpeer_id   = %s\n", t->peer_id);
    printf("\tinfo_hash = %s\n", t->info_hash);
    printf("\tfilename  = %s\n", t->filename);
    if (t->trackers != NULL) {
        printf("\tTrackers we know:\n");
        for (tracker_t *tr = t->trackers; tr != NULL; tr = tr->next)
            printf("\t\t%s\n", tr->url);
    }
    if (t->peers != NULL) {
        printf("\tPeers we know of:\n");
        for (peers_t *p = t->peers; p != NULL; p = p->next)
            printf("\t\t%s\t%s\t%s\n", p->id, p->ip, p->port);
    }
    if (t->pieces != NULL) {
        printf("\tChunks we need:\n");
        for (chunk_t *c = t->pieces; c != NULL; c = c->next)
            printf("\t\t%lli\t%s\n", c->num, c->checksum);
    }
    printf("\tpiece_len = %lli\n", t->piece_len);
    printf("\tfile_len  = %lli\n", t->file_len);
    printf("\tport      = %s\n", t->port);
    printf("\tevent     = %s\n", t->event);
    printf("\tuploaded  = %lli\n", t->uploaded);
    printf("\tdloaded   = %lli\n", t->dloaded);
    printf("\tleft      = %lli\n", t->left);
}

void
free_torrent(torrent_t *t)
{
    curl_free(t->info_hash);
    free(t->filename);
    if (t->trackers != NULL) {
        tracker_t *t_save;
        for (tracker_t *tp = t->trackers; tp != NULL; tp = t_save) {
            curl_free(tp->url);
            t_save = tp->next;
            free(tp);
        }
    }
    if (t->peers != NULL) {
        peers_t *p_save;
        for (peers_t *pp = t->peers; pp != NULL; pp = p_save) {
            free(pp->id);
            free(pp->ip);
            free(pp->port);
            p_save = pp->next;
            free(pp);
        }
    }
    if (t->pieces != NULL) {
        chunk_t *c_save;
        for (chunk_t *cp = t->pieces; cp != NULL; cp = c_save) {
            free(cp->checksum);
            c_save = cp->next;
            free(cp);
        }
    }
    free(t);
}

int
main(int argc, char *argv[])
{
    torrent_t *t      = NULL;
    char *     magnet = NULL;

    if (argc < 2) {
        FATAL("%s", USAGE);
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            log_verbosely = 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("%s", USAGE);
            return 0;
        } else {
            if (!strncmp("magnet:", argv[i], 7)) {
                if (magnet == NULL) magnet = argv[i];
                else continue;
            } 
        }
    }

    /* Get information from the URL */
    if ((t = magnet_parse_uri(magnet)) == NULL) {
        FATAL("Failed to parse magent link\n");
        FATAL("%s", USAGE);
        return -1;
    }

    /* Fill out the fields of the torrent struct with info from a tracker */
    t->peer_id = "-PC0001-478269329936";
    t->port    = "6881";
    t->event   = "started";
    if (magnet_request_tracker(t) < 0) {
        FATAL("Failed to get information from the tracker\n");
        return -1;
    }

    /* In order for the seeder and leacher to work, T must be full */
    if (log_verbosely) print_torrent(t);

    /* Now we can start a seeder and a leacher thread :3 */

    free_torrent(t);

    return 0;
}
