/* 
 * extract.c --- extract info from, then free, the metainfo structs
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>        /* inb4 libressl nerds */
#include <curl/curl.h>          /* For URLencoding */

#include "bitclient.h"
#include "extract.h"

/******************** H E R E   B E   D R A G O N S ********************/

/**
 * Helper function to extract the announce URL from the torrent data.
 * It takes a pointer to a the "announce" entry of the main dictionary,
 * a torrent_t struct into which to insert the value, and it returns
 * 0 on success.
 */
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

    be_free(d->val);

    return 0;
}

/**
 * Helper function to extract the name of the file we're downloading from
 * the info dictionary. It takes a pointer to the dictionary entry and one
 * to the torrent_t struct into which to insert the name. Returns 0 on success
 */
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
 * Helper function to extract the peices' sha1 hashes from D's value field
 * and place their hexadecimal digests into a linked list in T. Returns 0
 * on success.
 *
 * The value field in this dictionary is a concatenation of 20-byte piece
 * sha1 hashes. Note that this logic assumes a single-file torrent.
 */
static int
extract_info_pieces(be_dict_t *d, torrent_t *t)
{
    if (d == NULL || t == NULL) return 1;

    be_num_t piece   = 0;
    char    *baseptr = d->val->x.str.buf;
    char    *tmpbuf  = NULL;
    chunk_t *node    = NULL;
    chunk_t *prev    = NULL;

    /* Loop through the string by 20-byte intervals */
    while ((baseptr - d->val->x.str.buf) < d->val->x.str.len) {
        /* Copy a 20-byte SHA1 checksum into a temporary buffer */
        if ((tmpbuf = (char*)calloc(22, sizeof(char))) == NULL) {
            perror("calloc");
            return 1;
        }
        if ((tmpbuf = (char*)memcpy(tmpbuf, baseptr, 20)) == NULL) {
            perror("memcpy");
            return 1;
        }
        tmpbuf[21] = '\0';

        /* Allocate a node for the current chunk in the linked list */
        if ((node = (chunk_t*)calloc(1, sizeof(chunk_t))) == NULL) {
            perror("calloc");
            return 1;
        }

        node->num = piece;
        node->checksum = tmpbuf;

        /* Append the current node to the linked list of pieces */
        if (prev == NULL) {
            t->pieces = node;
            prev = node;
        } else {
            prev->next = node;
            prev = node;
        }        

        piece++;
        baseptr+=20;
    }
    return 0;
}

/**
 * Return a URLencoded SHA1 hash of the bencoded info dictionary.
 * This requires bencoding the entire sub-dictionary under the "info" key
 * (passed as infoval) then computing the SHA1 hash of the resulting
 * buffer. We then URLencode this hash and store it in t->info_hash.
 * Return 0 iff all goes well.
 */
static int
extract_info_hash(be_dict_t *infoval, torrent_t *t)
{
    CURL *curl;
    char *buf = NULL;
    char *url = NULL;
    unsigned char *sha = NULL;

    DEBUG("Attempting to extract info hash...\n");

    /* We need to stick infoval inside a be_node_t in order to encode it */
    be_node_t *node = NULL;
    if ((node = (be_node_t*)calloc(1, sizeof(be_node_t))) == NULL) {
        perror("calloc");
        return 1;
    }
    node->type = DICT;
    node->x.dict_head = infoval->link; /* Is be_encode struggling with this? */
    /* vvv these fuckers didn't work but I'll keep em around anywhay vvv */
    /* init_list_head(&infoval->link); */
    /* list_add_tail(&infoval->link, &node->x.dict_head); */

    /* Get the size of the required buffer */
    /* be_num_t len = be_encode(node, NULL, 0); */
    be_num_t len = 10000;    /* ^ won't work b/c bug, so big fucking number */

    /* Allocate a buffer in which to store the encoded data */
    if ((buf = (char*)calloc(len, sizeof(char))) == NULL) {
        perror("calloc");
        return 1;
    }

    /* Bencode the dictionary, saving the result in buf */
    if (be_encode(node, buf, len) == 0) {
        perror("be_encode");
        return 1;
    }

    /* Now make the aforementioned big fucking number smaller for SHA1 */
    len = strlen(buf);

    /* Allocate a buffer for the checksum */
    if ((sha = (unsigned char*)malloc(20)) == NULL) {
        perror("malloc");
        return 1;
    }

    /* Compute the checksum */
    if ((sha = SHA1((unsigned char*)buf, len, sha)) == NULL) {
        perror("SHA1");
        return 1;
    }

    /* Allocate a buffer to store the URLencoded checksum in */
    if ((url = (char*)calloc(100, sizeof(char))) == NULL) {
        perror("calloc");
        return 1;
    }

    /* URLencode the checksum */
    if ((curl = curl_easy_init()) != NULL) {
        if ((url = curl_easy_escape(curl, (char*)sha, 20)) == NULL) {
            perror("curl_easy_escape");
            return 1;
        }
    } else {
        perror("curl_easy_init");
        return 1;
    }

    /* Resize the buffer of URLencoded data */
    if ((url = (char*)realloc(url, strlen(url))) == NULL) {
        perror("realloc");
        return 1;
    }

    t->info_hash = (char*)url;

    free(buf);
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
     * Torrent metadata are stored in a dictionary (be_dict_t) where the 
     * key is some standard string and the value can be any of the following
     * types encapsulated inside a be_node_t. be_node_t's contain an enum
     * specifying the data a type and a union containing the data:
     *
     *   STR:  a bencode string
     *         union is be_str_t containing a string and its length
     *   NUM:  a bencode integer
     *         union is a long long int (typedef'd here to be_num_t for 
     *         brevity)
     *   DICT: a bencode dictionary
     *         union is a list_t pointing to the head of a doubly linked list
     *   LIST: a bencode list
     *         union is a list_t pointing to the head of a doubly linked list
     *
     * The bencode library uses intrusively linked lists which I'm traversing
     * using macros defined in bencode/list.h. Apparently this is what they
     * use in the Linux Kernel but it makes me *profoundly* uncomfortable.
     */

    DEBUG("Entering main metainfo dictionary\n");

    list_for_each(top_position, &t->data->x.dict_head) {
        e = list_entry(top_position, be_dict_t, link);

        if (!strncmp(e->key.buf, "announce", (size_t)e->key.len)) {
            if (extract_announce(e, t) != 0) {
                perror("extract_announce");
                return 1;
            }

        } else if (!strncmp(e->key.buf, "info", e->key.len)) {
            DEBUG("Entering info metainfo dictionary\n");
            list_for_each(info_position, &e->val->x.dict_head) {
                se = list_entry(info_position, be_dict_t, link);

                if (!strncmp(se->key.buf, "length", (size_t)se->key.len)) {
                    t->file_len = se->val->x.num;

                } else if (!strncmp(se->key.buf, "name", (size_t)se->key.len)) {
                    if (extract_info_name(se, t) != 0) {
                        perror("extract_info_name");
                        return 1;
                    }

                } else if (!strncmp(se->key.buf, "piece length", (size_t)se->key.len)) {
                    t->piece_len = se->val->x.num;

                } else if (!strncmp(se->key.buf, "pieces", (size_t)se->key.len)) {
                    if (extract_info_pieces(se, t) != 0) {
                        perror("extract_info_pieces");
                        return 1;
                    }
                    DEBUG("Successfully extracted piece checksums\n");
                    if (extract_info_hash(se, t) != 0) {
                        perror("extract_info_hash");
                        return 1;
                    }
                    DEBUG("Successfully extracted info hash\n");
                }
            }
            be_dict_free(se);

        } else if (!strncmp(e->key.buf, "announce-list", (size_t)e->key.len)) {
            be_free(e->val);
            continue;

        } else if (!strncmp(e->key.buf, "creation date", (size_t)e->key.len)) {
            be_free(e->val);
            continue;

        } else if (!strncmp(e->key.buf, "comment", (size_t)e->key.len)) {
            be_free(e->val);
            continue;

        } else if (!strncmp(e->key.buf, "created by", (size_t)e->key.len)) {
            be_free(e->val);
            continue;

        } else if (!strncmp(e->key.buf, "encoding", (size_t)e->key.len)) {
            be_free(e->val);
            continue;
        }
    }
    be_dict_free(e);

    return 0;
}
