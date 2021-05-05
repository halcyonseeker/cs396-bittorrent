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
#include <curl/curl.h>

#include "bitclient.h"
/* #include "extract.h" */
/* #include "tracker.h" */

#include "bencode/list.h"
#include "bencode/bencode.h"

int log_verbosely = 0;

#define USAGE                                                                  \
    "\
Usage: bitclient [-vh] magnet:\n\
    Options:\n\
        -v || --verbose        Log debugging information\n\
        -h || --help           Print this message and exit\n"


/**
 * Take a magnet link, and parse its contents into the torrent structure
 */
torrent_t *
parse_magnet(char *magnet)
{
    torrent_t *t     = NULL;
    tracker_t *curr  = NULL;
    tracker_t *prev  = NULL;
    char      *token = NULL;

    if (magnet == NULL) return NULL;

    /* Allocate the torrent struct */
    if ((t = (torrent_t*)calloc(1, sizeof(torrent_t))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Increment the magnet pointer to the first '?' */
    if ((magnet = strchr(magnet, '?') + 1) == NULL) {
        FATAL("Invalid magnet URL\n");
        return NULL;
    }

    char *div = NULL;
    /* Extract the key-value pairs from the magnet */
    while ((div = strchr(magnet, '&')) != NULL) {
        if ((token = strndup(magnet, (size_t)(div - magnet))) == NULL) {
            perror("strndup");
            return NULL;
        }

        if (!strncmp("xt", token, 2)) {        /* File hash */
            /* TODO: make the hash not hexadecimal */
            if ((t->info_hash = strdup(strrchr(token, ':') + 1)) == NULL) {
                perror("strdup");
                return NULL;
            }
            
        } else if (!strncmp("dn", token, 2)) { /* Torrent name */
            if ((t->filename = strdup(token + 3)) == NULL) {
                perror("strdup");
                return NULL;
            }
            
        } else if (!strncmp("tr", token, 2)) { /* URLencoded tracker URL */
            if ((curr = (tracker_t*)calloc(1, sizeof(tracker_t))) == NULL) {
                perror("calloc");
                return NULL;
            }

            /* Decode the URL */
            CURL *curl  = curl_easy_init();
            if (curl) {
                curr->url = curl_easy_unescape(curl, token+3, strlen(token)-2, NULL);
                if (curr->url == NULL) {
                    perror("curl-easy_unescape");
                    return NULL;
                }
            }

            /* Append to the announce linked list */
            if (prev == NULL) {
                t->trackers = curr;
                prev = curr;
            } else {
                prev->next = curr;
                prev = curr;
            }
        }
        magnet = div + 1;
    } 
    return t;
}

int
main(int argc, char *argv[])
{
    torrent_t *t      = NULL;
    char *     magnet = NULL;

    if (argc < 2) {
        FATAL("%s", USAGE);
        FATAL("1\n");
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
    if ((t = parse_magnet(magnet)) == NULL) {
        FATAL("Failed to parse magent link\n");
    }

    if (t == NULL) {
        FATAL("%s", USAGE);
        FATAL("3\n");
        return -1;
    }

    /* Generate a peer ID */

    /* Ask each of the trackers for peer information */

    printf("Torrent structure:\n");
    printf("\tpeer_id   = %s\n", t->peer_id);
    printf("\tinfo_hash = %s\n", t->info_hash);
    printf("\tfilename  = %s\n", t->filename);
    if (t->trackers != NULL) {
        printf("\tTrackers we know:\n");
        for (tracker_t *tr = t->trackers; tr->next != NULL; tr = tr->next)
            printf("\t\t%s\n", tr->url);
    }
    if (t->peers != NULL) {
        printf("\tPeers we know of:\n");
        for (peers_t *p = t->peers; p->next != NULL; p = p->next)
            printf("\t\t%s\t%s\t%s\n", p->id, p->ip, p->port);
    }
    if (t->pieces != NULL) {
        printf("\tChunks we need:\n");
        for (chunk_t *c = t->pieces; c->next != NULL; c = c->next)
            printf("\t\t%lli\t%s\n", c->num, c->checksum);
    }
    /* printf("\tpiece_len = %lli\n", t->piece_len); */
    /* printf("\tfile_len  = %lli\n", t->file_len); */
    /* printf("\tport      = %s\n", t->port); */
    /* printf("\tevent     = %s\n", t->event); */
    /* printf("\tuploaded  = %lli\n", t->uploaded); */
    /* printf("\tdloaded   = %lli\n", t->dloaded); */
    /* printf("\tleft      = %lli\n", t->left); */

    /* Now we can start a seeder and a leacher thread :3 */

    return 0;
}
