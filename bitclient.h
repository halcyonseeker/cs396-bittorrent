/* 
 * bitclient.h --- Macros and types that will be used in multiple files
 */

#pragma once 

#include "bencode/bencode.h"
#include "bencode/list.h"

#define FATAL(...)                                                             \
    fputs("\033[1;38;5;1mFATAL\033[m: ", stderr);                              \
    fprintf(stderr, __VA_ARGS__);

#define DEBUG(...)                                                             \
    if (log_verbosely) {                                                       \
        fputs("\033[1;38;5;6mDEBUG\033[m: ", stderr);                          \
        fprintf(stderr, __VA_ARGS__);                                          \
    }

extern int log_verbosely;

/**
 * Torrent files are passed on the command line, we'll store them in a
 * linked list of torrent_t structs. The data field is a pointer to the
 * data from the .torrent file and the other fields contain the bencoded
 * information that we'll actually use. The chunks_t struct contains the
 * the information required to validate and serialize the data we get
 * from peers.
 */
typedef long long int be_num_t;

typedef struct chunk {
    be_num_t       num;
    char         *checksum;
    struct chunk *next;
} chunk_t;

typedef struct torrent {
    be_node_t *     data;
    be_num_t        piece_len;
    be_num_t        file_len;
    chunk_t *       pieces;
    char *          filename;
    char *          announce;
    struct torrent *next;
} torrent_t;
