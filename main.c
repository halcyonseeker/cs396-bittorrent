/*
 * main.c: A simple BitTorrent client
 * Thalia Wright <wrightng@reed.edu>
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bencode/bencode.h"

#define USAGE                                                                  \
    "\
Usage: bitclient [-vh] file1.torrent, ...\n\
    Options:\n\
        -v || --verbose        Log debugging information\n\
        -h || --help           Print this message and exit\n"

#define FATAL(...)                                                             \
    fputs("\033[1;38;5;1mFATAL\033[m: ", stderr);                              \
    fprintf(stderr, __VA_ARGS__);

#define DEBUG(...)                                                             \
    if (log_verbosely) {                                                       \
        fputs("\033[1;38;5;6mDEBUG\033[m: ", stderr);                          \
        fprintf(stderr, __VA_ARGS__);                                          \
    }

static int log_verbosely = 0;

/**
 * Torrent files are passed on the command line, we'll store them in a
 * linked list of torrent_t structs. The data field is a pointer to the
 * data from the .torrent file and the other fields contain the bencoded
 * information that we'll actually use. The chunks_t struct contains the
 * the information required to validate and serialize the data we get
 * from peers.
 */
typedef long long int be_num_t;

typedef struct chunks {
    int             num;
    char           *checksum;
    struct chunks *next;
} chunks_t;

typedef struct torrent {
    be_node_t *     data;
    be_num_t        piece_len;
    be_num_t        file_len;
    chunks_t *      pieces;
    char *          filename;
    char *          announce;
    struct torrent *next;
} torrent_t;

/**
 * Start the process of downloading a torrent. Takes a pointer to a .torrent
 * file. Returns a pointer to the torrent file if everything went okay and
 * NULL on failure.
 */
void *
thread_main(void *raw)
{
    torrent_t *torrent = (torrent_t *)raw;

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

    fclose(fp);
    return t;
}

/*
 * https://en.wikipedia.org/wiki/BitTorrent
 * https://wiki.theory.org/BitTorrentSpecification
 * http://www.bittorrent.org/beps/bep_0003.html
 * https://www.morehawes.co.uk/the-bittorrent-protocol
 * https://www.beautifulcode.co/blog/58-understanding-bittorrent-protocol
 * http://dandylife.net/docs/BitTorrent-Protocol.pdf
 * https://skerritt.blog/bit-torrent/
 */
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
                DEBUG("Successfully parsed torrent file %s\n", argv[i]);
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

    /*
     * Bencode data are stored in a doubly linked list. Each node contains a
     * be_type enum specifying the type of data in this node, and a union
     * containing said data:
     * be_type:
     *   STR:  a bencode string
     *         union is be_str_t containing a string and its length
     *   NUM:  a bencode integer
     *         union is a long long int
     *   LIST: a bencode list
     *         union is a list_t pointing to the head of a doubly linked list
     *         --- FIXME: where is the data of the linked list??
     *   DICT: a bencode dictionary
     *         union is a list_t pointing to the head of a doubly linked list
     *         --- FIXME where is the data of the linked list, the be_dict_t??
     */

    /* Launch a new thread for each torrent */
    for (torrent_t *t = torrent_head; t != NULL; t = t->next, tnum++) {
        if (pthread_create(&threads[tnum], NULL, thread_main, t) != 0) {
            perror("pthread_create");
            return 1;
        } else {
            DEBUG("Launched torrent thread #%i\n", tnum);
        }
    }

    /* Join the threads */
    for (tnum = 0; tnum < nthreads; tnum++) {
        if (pthread_join(threads[tnum], NULL) != 0) {
            perror("pthread_join");
            return 1;
        } else {
            DEBUG("Joined torrent thread #%i with main\n", tnum);
        }
    }

    /* TODO: free the linked list and decoded data */

    return 0;
}
