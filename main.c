/*
 * main.c: A simple BitTorrent client
 * Thalia Wright <wrightng@reed.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

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
struct torrent {
    be_node_t *data;
    struct torrent *next;
};

typedef struct torrent torrent_t;

/**
 * Take a decoded .torrent file and begin the process of downloading the file.
 * If the file is successfully downloaded, return a pointer to it.
 */
FILE *
download_file(be_node_t *data)
{
    FILE *fp = NULL;

    /* TODO Read the data and do stuff */

    return fp;
}

/**
 * Wrap up the process of downloading and saving a file. For each file we're
 * downloading, one of these gets launched in a separate thread.
 */
void *
torrent_thread_main(void *data)
{
    FILE *file = NULL;
    FILE *save = NULL;
}

/**  
 * Take the filename of a torrent file, open it, and parse to the torrent
 * struct. Return a pointer to it iff it is successfully opened and parsed.
 */
torrent_t *
open_torrent(char *filename)
{
    torrent_t *t = NULL;
    char *buf = NULL;
    FILE *fp = NULL;
    long buflen = 0;
    size_t read_amount;

    vlog("Parsing torrent file %s...\n", filename);

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
    if ((t = calloc(1, sizeof(torrent_t))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Read the torrent file into buf for passing to be_decode() */
    if (fread((void*)buf, sizeof(char), buflen, fp) < (size_t)buflen - 1) {
        perror("fread");
        return NULL;
    }

    /* Decode the file */
    if ((t->data = be_decode(buf, buflen, &read_amount)) == NULL) {
        perror("be_decode");
        return NULL;
    }

    fclose(fp);
    free(buf);
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
    torrent_t *torrent_head = NULL, *torrent_current = NULL;

    struct thread_info tinfo;
    pthread_attr_t tattr;
    void * tret;
    
    if (argc < 2) {
        fputs(USAGE, stderr);
        dbg("No arguments were provided\n");
        return 1;
    }

    /* Check for arguments and build the linked list of torrents */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            log_verbosely = 1;
            vlog("Verbose logging active\n");
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            fputs(USAGE, stdout);
            return 0;
        } else {
            if ((torrent_current = open_torrent(argv[i])) != NULL) {
                if (torrent_head == NULL) torrent_head = torrent_current;
                vlog("Successfully parsed torrent file %s\n", argv[i]);
            } else {
                fputs(USAGE, stderr);
                dbg("open_torrent() returned NULL\n");
                dbg("A file either doesn't exist or is garbled\n");
                return 1;
            }
        }
    }

    if (torrent_head == NULL) {
        fputs(USAGE, stderr);
        dbg("torrent_head is NULL\n");
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

    /* 
     * FIXME: This thread stuff is probably shit, figure out the logic
     *        and learn more about pthreads
     */

    /* Initialize the thread attributes */
    if (pthread_attr_init(&tattr) != 0) {
        perror("pthread_attr_init");
        return 1;
    }
    
    /* Launch some threads, one for each torrent file */
    int tnum = 0;
    for (torrent_t *t = torrent_head; t != NULL; t = t->next, tnum++) {
        vlog("Launching thread %i...\n", tnum);
        if (pthread_create(&tinfo[tnum].thread_id,
                           &tattr,
                           &torrent_thread_main,
                           (void*)t->data) != 0) {
            perror("pthread_create");
            dbg("Failed to launch thread %i\n", tnum);
        }
    }

    /* Destroy the thread attributes */
    if (pthread_attr_destroy(&attr) != 0) {
        perror("pthread_attr_destroy");
        return 1;
    }

    /* Join the threads together */
    tnum = 0;
    for (torrent_t *t = torrent_head; t != NULL; t = t->next, tnum++) {
        if (pthread_join(tinfo[tnum].thread_id, tret) != 0) {
            perror("pthread_join");
            dbg("Failed to download torrent %i\n", tnum);
            continue;
        }
        vlog("Successfully downloaded torrent %i\n", tnum);
        free(res);
    }

    /* 
     * FIXME: this leaks hella memory
     * TODO: submit PR to add a be_list_free() function to bencode library? 
     * TODO: integrate this with the pthread_join loop
     */

    torrent_t *ts = NULL;
    for (torrent_t *t = torrent_head; t != NULL; t = ts) {

        if (t->data != NULL) {
            be_node_t *bs = NULL;
            for (be_node_t *b = t->data; b != NULL; b = bs) {
                bs = b->link->next;
                be_free(b);
            }
        }
        free(t);
    }

    return 0;
}
