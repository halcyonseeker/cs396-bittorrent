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

#include "bencode/list.h"
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

static int
extract_announce(be_dict_t *d, torrent_t *t)
{
    if (d == NULL || t == NULL) return 1;

    t->announce = (char*)calloc(d->val->x.str.len + 1, sizeof(char));
    if (t->announce == NULL) {
        perror("calloc");
        return errno;
    }

    t->announce = strncpy(t->announce, d->val->x.str.buf, d->val->x.str.len);
    if (t->announce == NULL) {
        perror("strncpy");
        return errno;
    }

    return 0;
}

static int
extract_info_name(be_dict_t *d, torrent_t *t)
{
    if (d == NULL || t == NULL) return 1;

    t->filename = (char*)calloc(d->val->x.str.len + 1, sizeof(char));
    if (t->filename == NULL) {
        perror("calloc");
        return errno;
    }

    t->filename = strncpy(t->filename, d->val->x.str.buf, d->val->x.str.len);
    if (t->filename == NULL) {
        perror("strncpy");
        return errno;
    }

    return 0;
}

/**
 * The value field in this dictionary is a concatenation of 20-byte piece
 * sha1 hashes. Note that this logic assumes a single-file torrent.
 */
static int
extract_info_pieces(be_dict_t *d, torrent_t *t)
{
    if (d == NULL || t == NULL) return 1;

    be_num_t pnum   = 0;
    be_num_t offset = 0;
    chunk_t *head   = NULL;
    chunk_t *curr   = NULL;
    char    *begin  = d->val->x.str.buf;

    while (offset < d->val->x.str.len) {
        curr = (chunk_t*)calloc(1, sizeof(chunk_t));
        if (curr == NULL) {
            perror("calloc");
            return errno;
        }

        curr->checksum = (char*)calloc(22, sizeof(char));
        if (curr->checksum == NULL) {
            perror("calloc");
            return errno;
        }

        curr->checksum = strncpy(curr->checksum, begin, 20);
        if (curr->checksum == NULL) {
            perror("strncpy");
            return errno;
        }

        curr->checksum[21] = '\0';
        curr->num = pnum;

        if (head == NULL) {
            head = curr;
        } else {
            curr->next = head;
            head       = curr;
        }

        pnum++;
        offset+=20;
    }
    t->pieces = head;

    return 0;
}


/**
 * "My name is extract_from_bencode, extractor of important data:
 *  Look on my horrible spaghetti, ye Mighty, and despair!"
 *         -- Percy Shelly, Ozymandias
 *
 * .torrent files can contain a lot of information, this functions takes
 * a torrent_t struct which contains a pointer to the bencode structures
 * and fields for important information. It then traverses the bencode
 * structures and copies the important information into the argument's
 * other fields.
 *
 * The relevant fields are described here:
 * https://wiki.theory.org/BitTorrentSpecification#Metainfo_File_Structure
 */
int
extract_from_bencode(torrent_t *t)
{
    list_t *top_position, *info_position;
    be_dict_t *e, *se;

    if (t == NULL) return 1;

    /*
    * The bencode library uses intrusively linked lists which I'm traversing
    * using macros defined in bencode/list.h. Apparently this is what they
    * use in the Linux Kernel but it makes me *profoundly* uncomfortable.
    */

    list_for_each(top_position, &t->data->x.dict_head) {
        e = list_entry(top_position, be_dict_t, link);

        int ra  = strncmp(e->key.buf, "announce", (size_t)e->key.len);
        int ri  = strncmp(e->key.buf, "info", e->key.len);
        /* Optional */
        int ral = strncmp(e->key.buf, "announce-list", (size_t)e->key.len);
        int rcd = strncmp(e->key.buf, "creation date", (size_t)e->key.len);
        int rc  = strncmp(e->key.buf, "comment", (size_t)e->key.len);
        int rcb = strncmp(e->key.buf, "created by", (size_t)e->key.len);
        int re  = strncmp(e->key.buf, "encoding", (size_t)e->key.len);

        if (ra == 0) {         /* announce */
            if (extract_announce(e, t) != 0) {
                perror("extract_announce");
                return 1;
            }

        } else if (ri == 0) {   /* info */
            list_for_each(info_position, &e->val->x.dict_head) {
                se = list_entry(info_position, be_dict_t, link);

                int ril  = strncmp(se->key.buf, "length", (size_t)se->key.len);
                int rin  = strncmp(se->key.buf, "name", (size_t)se->key.len);
                int ripl = strncmp(se->key.buf, "piece length", (size_t)se->key.len);
                int rip  = strncmp(se->key.buf, "pieces", (size_t)se->key.len);

                if (ril == 0) {          /* length */
                    t->file_len = se->val->x.num;

                } else if (rin == 0) {   /* name */
                    if (extract_info_name(se, t) != 0) {
                        perror("extract_info_name");
                        return 1;
                    }

                } else if (ripl == 0) {  /* piece length */
                    t->piece_len = se->val->x.num;

                } else if (rip == 0) {   /* pieces */
                    if (extract_info_pieces(se, t) != 0) {
                        perror("extract_info_pieces");
                        return 1;
                    }
                }
            }

        } else if (ral == 0) {  /* announce-list (optional) */
            continue;

        } else if (rcd == 0) {  /* creation date (optional) */
            continue;

        } else if (rc == 0) {   /* comment (optional */
            continue;

        } else if (rcb == 0) {  /* created by (optional) */
            continue;

        } else if (re == 0) {   /* encoding (optional) */
            continue;
        }
    }

    be_dict_free(e);            /* Free outer dictionary */

    return 0;
}

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

    printf("piece_len = %lli\nfile_len  = %lli\nfilename  = %s\nannounce  = %s\n",
           torrent->piece_len,
           torrent->file_len,
           torrent->filename,
           torrent->announce);

    /* printf("Checksums:\n"); */
    /* for (chunk_t *c = torrent->pieces; c != NULL; c = c->next) */
    /*     printf("\t%lli\t%s\n", c->num, c->checksum); */

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
     *   DICT: a bencode dictionary
     *         union is a list_t pointing to the head of a doubly linked list
     * Note that this bencode library uses a Linux-style intrusive linked list
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
