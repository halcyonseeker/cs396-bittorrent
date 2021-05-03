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

/* Store the tracker's response */
typedef struct tracker {
    char *   id;         /* A string I should use to identify myself */
    be_num_t interval;   /* Number of seconds to wait before sending a request */
    be_num_t complete;   /* Number of peers with full file */
    be_num_t leechers;   /* Number of peers with incomplete file */
    peers_t *peers;      /* A linked list of peer info */
} tracker_t;

/* Store a torrent's piece checksums */
typedef struct chunk {
    be_num_t       num;
    char         *checksum;
    struct chunk *next;
} chunk_t;

/* Store information about the torrent specified on the command line */
typedef struct torrent {
    be_node_t *     data;      /* Decoded metainfo file */
    /* Information from the metainfo file */
    be_num_t        piece_len; /* The length of pieces the file is split into */
    be_num_t        file_len;  /* The lendth of the file we're downloading */
    chunk_t *       pieces;    /* A linked list of piece numbers and hashes */
    char *          filename;  /* The name of the file we should save */
    char *          announce;  /* The tracker URL. We only support 1 for now */
    char *          info_hash; /* A sha1 hash of the pieces hash string */
    /* Look Ma, I'm a peer now! */
    char *          peer_id;   /* Who am I? No seriously, who am I really? */
    char *          port;      /* The port we'll listen on */
    char *          event;     /* What we're doing rn */
    be_num_t        uploaded;  /* How many bytes we've uploaded */
    be_num_t        dloaded;   /* How many bytes we've downloaded */
    be_num_t        left;      /* The number of bytes we still need */
    be_num_t        compact;   /* No */
    /* Information from the announce tracker */
    tracker_t *     trackers;  /* Some nice folks we should talk with */
    struct torrent *next;      /* We shouldn't support multiple torrents */
} torrent_t;
