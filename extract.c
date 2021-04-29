/* 
 * extract.c --- extract info from, then free, the metainfo structs
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extract.h"
#include "bitclient.h"

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
            be_dict_free(se);

        } else if (ral == 0) {  /* announce-list (optional) */
            be_free(e->val);
            continue;

        } else if (rcd == 0) {  /* creation date (optional) */
            be_free(e->val);
            continue;

        } else if (rc == 0) {   /* comment (optional */
            be_free(e->val);
            continue;

        } else if (rcb == 0) {  /* created by (optional) */
            be_free(e->val);
            continue;

        } else if (re == 0) {   /* encoding (optional) */
            be_free(e->val);
            continue;
        }
    }
    be_dict_free(e);

    return 0;
}
