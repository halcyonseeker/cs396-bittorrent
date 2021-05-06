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

typedef long long int be_num_t;

/* Store information about the peers */
typedef struct peers {
    char  *id;           /* The peer's self-selected id */
    char  *ip;           /* Duh */
    char  *port;         /* Duh */
    struct peers *next;  /* I'm tired of linked lists */
} peers_t;

/* Store tracker URLs in a linked list */
typedef struct tracker {
    char *          url;
    struct tracker *next;
} tracker_t;

/* Store a torrent's piece checksums */
typedef struct chunk {
    be_num_t      num;
    char         *checksum;
    struct chunk *next;
} chunk_t;

/* Store information about the torrent specified on the command line */
typedef struct torrent {
    /* Information we need in order to become a peer */
    char *     peer_id;   /* A hash to id myself when talking with peers */
    char *     info_hash; /* A unique id for the torrent we're transferring */
    char *     filename;  /* The name of the file we'll save */
    tracker_t *trackers;  /* These guys tell us where to find peers */
    peers_t *  peers;     /* Some nice folks we'll share chunks with */
    chunk_t *  pieces;    /* Checksums and numbers so we build the file right */
    be_num_t   piece_len; /* Bytes per chunk */
    be_num_t   file_len;  /* Bytes in the file */
    /* Look Ma, I'm a peer now! */
    char *     port;      /* Where we're listening */
    char *     event;     /* What we're doing */
    be_num_t   uploaded;  /* Bytes we've uploaded */
    be_num_t   dloaded;   /* Bytes we've downloaded */
    be_num_t   left;      /* Bytes we still need */
    /* Required for UDP trackers */
    uint32_t   trans_id;  /* Our unique 32-bit ID, big endian */
    uint32_t   conn_id;   /* An announce session ID, big endian */
} torrent_t;
