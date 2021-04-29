/* 
 * extract.c --- extract info from, then free, the metainfo structs
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extract.h"
#include "bitclient.h"

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

        curr->checksum = extract_hex_digest(begin, 20);
        if (curr->checksum == NULL) {
            perror("extract_hex_digest");
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
 * Take a buffer of binary data and convert it to a hexadecimal string.
 * This function is used both internally in extract.c to save the piece
 * hashes in a printable form, and externally to compare sha1 hashes.
 */
char *
extract_hex_digest(char *buf, int len)
{
    char *hexbuf  = (char*)calloc(len + 22, sizeof(char));
    char *endptr  = buf + len;
    long long hex = strtoll(buf, &endptr, 2);

    sprintf(hexbuf, "%llx", hex);

    return hexbuf;
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

    list_for_each(top_position, &t->data->x.dict_head) {
        e = list_entry(top_position, be_dict_t, link);

        if (!strncmp(e->key.buf, "announce", (size_t)e->key.len)) {
            if (extract_announce(e, t) != 0) {
                perror("extract_announce");
                return 1;
            }

        } else if (!strncmp(e->key.buf, "info", e->key.len)) {
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
