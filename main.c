/*
 * main.c: A simple BitTorrent client
 * Thalia Wright <wrightng@reed.edu>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bencode/bencode.h"

#define USAGE "\
Usage: bitclient [-vh] file1.torrent, ...\n\
    Options:\n\
        -v || --verbose        Log verbosely\n\
        -h || --help           Print this message and exit\n"

#define dbg(...)                                                \
    fputs("\033[1;38;5;9mDEBUG\033[m: ", stderr);               \
    fprintf(stderr, __VA_ARGS__);

#define vlog(...)                                               \
    if (log_verbosely) {                                        \
        fputs("\033[1;38;5;6mLOG\033[m: ", stdout);             \
        printf(__VA_ARGS__);                                    \
    }

static int log_verbosely = 0;

/** 
 * Torrent files are passed on the command line, we'll store them in a 
 * linked list. The data field is a pointer to the data from the .torrent file
 */
typedef struct {
    be_node_t *data;
    struct torrent *next;
} torrent;

/**  
 * Take the filename of a torrent file, open it, and parse to the torrent
 * struct. Return a pointer to it iff it is successfully opened and parsed.
 */
torrent *
open_torrent(char *filename)
{
    torrent *t = NULL;
    char *buf = NULL;
    FILE *fp = NULL;
    long buflen = 0;
    size_t read_amount;

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
    if ((buf = (char*)calloc(buflen, sizeof(char))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Allocate the torrent struct */
    if ((t = calloc(1, sizeof(torrent))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Read the torrent file into buf for passing to be_decode() */
    if (fread((void*)buf, sizeof(char), buflen, fp) < (size_t)buflen-1) {
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
 * https://en.wikipedia.org/wiki/Magnet_URI_scheme
 * https://wiki.theory.org/BitTorrentSpecification
 */
int
main(int argc, char *argv[])
{
    torrent *torrent_head = NULL, *torrent_current = NULL;
    
    if (argc < 2) {
        fputs(USAGE, stderr);
        dbg("No arguments were provided");
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
                if (torrent_head == NULL) torrent_head = torrent_current;
                vlog("Successfully parsed torrent file");
            } else {
                fputs(USAGE, stderr);
                dbg("open_torrent() returned NULL");
                fputs("A file either doesn't exist or is garbled\n", stderr);
                return 1;
            }
        }
    }

    if (torrent_head == NULL) {
        fputs(USAGE, stderr);
        dbg("torrent_head is NULL");
        return 1;
    }

    /* TODO: wtf king of data structure is be_node_t */

    /* TODO: do torrent things */

    /* TODO: free the linked list and decoded data */

    return 0;
}
