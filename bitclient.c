/*
 * main.c: A simple BitTorrent client
 * Thalia Wright <wrightng@reed.edu>
 * DUE MAY 10
 */

#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
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
 * Base conversion is fiddly and error-prone so I borrowed this from here:
 * https://github.com/transmission/transmission/blob/master/libtransmission/utils.c#L815
 */
void
hex_to_binary(void const* vinput, void* voutput, size_t byte_length)
{
    static char const hex[] = "0123456789abcdef";

    uint8_t const* input = (uint8_t const*)vinput;
    uint8_t* output = voutput;

    for (size_t i = 0; i < byte_length; ++i) {
        int const hi = strchr(hex, tolower(*input++)) - hex;
        int const lo = strchr(hex, tolower(*input++)) - hex;
        *output++ = (uint8_t)((hi << 4) | lo);
    }
}

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

    /* Extract the key-value pairs from the magnet */
    char *div = NULL;
    while ((div = strchr(magnet, '&')) != NULL) {
        if ((token = strndup(magnet, (size_t)(div - magnet))) == NULL) {
            perror("strndup");
            return NULL;
        }

        if (!strncmp("xt", token, 2)) {        /* File hash */
            char *hex, bin[100], *binptr = &bin[0];
            memset(bin, 0, 100);

            if ((hex = strdup(strrchr(token, ':') + 1)) == NULL) {
                perror("strdup");
                return NULL;
            }

            /* Hex is just too pretty, so we need URL-encoded binary :/ */
            hex_to_binary(hex, binptr, 20);
            CURL *curl = curl_easy_init();
            if (curl) {
                t->info_hash = curl_easy_escape(curl, binptr, strlen(binptr));
                if (t->info_hash == NULL) {
                    perror("curl_easy_unescape");
                    return NULL;
                }
                curl_easy_cleanup(curl);
            }

            free(hex);
            
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
                    perror("curl_easy_unescape");
                    return NULL;
                }
                curl_easy_cleanup(curl);
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
        free(token);
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
    t->peer_id = "12345678912345678900";

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

    /* Free the torrent */
    free(t->info_hash);
    free(t->filename);
    if (t->trackers != NULL) {
        tracker_t *t_save;
        for (tracker_t *tp = t->trackers; tp != NULL; tp = t_save) {
            curl_free(tp->url);
            t_save = tp->next;
        }
    }
    if (t->peers != NULL) {
        peers_t *  p_save;
        for (peers_t *pp = t->peers; pp != NULL; pp = p_save) {
            free(pp->id);
            free(pp->ip);
            free(pp->port);
            p_save = pp->next;
        }
    }
    if (t->pieces != NULL) {
        chunk_t *  c_save;
        for (chunk_t *cp = t->pieces; cp != NULL; cp = c_save) {
            free(cp->checksum);
            c_save = cp->next;
        }
    }
    free(t->port);
    free(t->event);
    free(t);

    return 0;
}
