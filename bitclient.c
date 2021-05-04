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

#include "bitclient.h"
/* #include "extract.h" */
/* #include "tracker.h" */

#include "bencode/list.h"
#include "bencode/bencode.h"

int log_verbosely = 0;

#define USAGE                                                                  \
    "\
Usage: bitclient [-vh] file1.torrent, ...\n\
    Options:\n\
        -v || --verbose        Log debugging information\n\
        -h || --help           Print this message and exit\n"


/**
 * Start the process of downloading a torrent. Takes a pointer to a .torrent
 * file. Returns a pointer to the torrent file if everything went okay and
 * NULL on failure.
 */
void *
thread_main(void *raw)
{
    torrent_t *torrent = (torrent_t *)raw;

    if (extract_from_bencode(torrent) != 0) {
        FATAL("A failure occurred in extract_from_bencode()");
        return NULL;
    }

    /* Set a "unique" 20-character ID */
    torrent->peer_id = "12345678912345678900";

    /* Set a port, we'll get a socket later */
    torrent->port = "6881";

    /* printf("piece_len = %lli\nfile_len  = %lli\nfilename  = %s\nannounce  = %s\n", */
    /*        torrent->piece_len, */
    /*        torrent->file_len, */
    /*        torrent->filename, */
    /*        torrent->announce); */

    /* for (chunk_t *c = torrent->pieces; c != NULL; c = c->next) */
    /*     printf("\t%lli\t0x%s\n", c->num, c->checksum); */

    /* For now this will just print out the message returned by the tracker */
    if (tracker_request_peers(torrent)) {
        FATAL("Unable to request peer list from tracker.\n");
        return NULL;
    }

    return torrent;
}

/**
 * Take the filename of a torrent file, open it, and parse to the torrent
 * struct. Return a pointer to it iff it is successfully opened and parsed.
 */
torrent_t *
open_torrent(char *filename)
{
    torrent_t *t      = NULL;
    char *     buf    = NULL;
    FILE *     fp     = NULL;
    long       buflen = 0;
    size_t     read_amount;

    /* Open the file */
    if ((fp = fopen(filename, "r")) == NULL) {
        perror("fopen");
        return NULL;
    }

    /* Get the length of the file for allocating a buffer */
    if (fseek(fp, 0L, SEEK_END) == -1) {
        perror("fseek");
        return NULL;
    }
    if ((buflen = ftell(fp)) < 0) {
        perror("ftell");
        return NULL;
    }

    /* Seek back to the beginning and account for \0 */
    rewind(fp);
    buflen++;

    /* Allocate the buffer to be passed to be_decode() */
    if ((buf = (char *)calloc(buflen, sizeof(char))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Allocate the torrent struct */
    if ((t = calloc(1, sizeof(torrent_t))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Read the torrent file into buf for passing to be_decode() */
    if (fread((void *)buf, sizeof(char), buflen, fp) < (size_t)buflen - 1) {
        perror("fread");
        return NULL;
    }

    /* Decode the file */
    if ((t->data = be_decode(buf, buflen, &read_amount)) == NULL) {
        perror("be_decode");
        return NULL;
    }

    free(buf);
    fclose(fp);
    return t;
}

int
main(int argc, char *argv[])
{
    torrent_t *torrent_head = NULL, *torrent_current = NULL;

    int nthreads = 0, tnum = 0;

    if (argc < 2) {
        fputs(USAGE, stderr);
        FATAL("No arguments were provided\n");
        return 1;
    }

    /* Check for arguments and build the linked list of torrents */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
            log_verbosely = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            fputs(USAGE, stdout);
            return 0;
        } else {
            if ((torrent_current = open_torrent(argv[i])) != NULL) {
                if (torrent_head == NULL)
                    torrent_head = torrent_current;
                else {
                    torrent_current->next = torrent_head;
                    torrent_head          = torrent_current;
                }
                nthreads++;
            } else {
                fputs(USAGE, stderr);
                FATAL("open_torrent() returned NULL\n");
                FATAL("A file either doesn't exist or is garbled\n");
                return 1;
            }
        }
    }

    pthread_t threads[nthreads];

    if (torrent_head == NULL) {
        fputs(USAGE, stderr);
        FATAL("torrent_head is NULL\n");
        return 1;
    }

    /* Launch a new thread for each torrent */
    for (torrent_t *t = torrent_head; t != NULL; t = t->next, tnum++) {
        if (pthread_create(&threads[tnum], NULL, thread_main, t) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    /* Join the threads */
    for (tnum = 0; tnum < nthreads; tnum++) {
        if (pthread_join(threads[tnum], NULL) != 0) {
            perror("pthread_join");
            return 1;
        }
    }

    DEBUG("Exiting cleanly...\n");

    /* Free the torrents */
    torrent_t *t_save;
    chunk_t *  c_save;
    peers_t *  p_save;
    for (torrent_t *t = torrent_head; t != NULL; t = t_save) {
        /* t->data should have been saved earlier */
        free(t->filename);
        free(t->announce);
        free(t->info_hash);
        if (t->trackers != NULL) {
            free(t->trackers->id);
            if (t->trackers->peers != NULL) {
                for (peers_t *p = t->trackers->peers; p != NULL; p = p_save) {
                    free(p->id);
                    free(p->ip);
                    free(p->port);
                    p_save = p->next;
                    free(p);
                }
            }
            free(t->trackers);
        }
        if (t->pieces != NULL) {
            for (chunk_t *c = t->pieces; c != NULL; c = c_save) {
                free(c->checksum);
                c_save = c->next;
                free(c);
            }
        }
        t_save = t->next;
        free(t);
    }

    return 0;
}
