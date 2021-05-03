/*
 * tracker.c -- Request information from and update trackers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "bitclient.h"
#include "tracker.h"

/**
 * See CURLOPT_WRITEFUNCTION(3)
 */
static size_t
curl_writefunc_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t len = nmemb * size;
    char *buf = (char*)userp;
    memcpy(userp, data, nmemb * size);
    if ((userp = (char*)realloc(userp, len + 1)) == NULL) {
        perror("realloc");
        return 1;
    }
    buf[len + 1] = '\0';
    return 0;
}

static char *
gen_api_url(torrent_t *t)
{
    /* We'll just return a test url for now */
    if (t == NULL) return NULL;
    return "http://ulthar.xyz:80";
/**
 * Take the bencoded dictionary returned by a tracker (passed in BUF) and
 * extract important information from it, returning it in a tracker_t struct.
 */
static tracker_t *
tracker_parse_response(char *buf)
{
    tracker_t *track = NULL;
    be_node_t *raw   = NULL;
    size_t     read_amount;

    be_dict_t *e, *se;
    list_t    *top_position, *peers_position;

    if (buf == NULL) return NULL;

    if ((track = (tracker_t*)calloc(1, sizeof(tracker_t))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Pass the buffer to the bencode decoder */
    if ((raw = be_decode(buf, strlen(buf), &read_amount)) == NULL) {
        perror("be_decode");
        return NULL;
    }

    /* Extract useful information from the decoded dictionary */
    /* See extract.c for explanatory comments on this horrible mess */
    list_for_each(top_position, &raw->x.dict_head) {
        e = list_entry(top_position, be_dict_t, link);

        if (!strncmp(e->key.buf, "failure reason", (size_t)e->key.len)) {
            FATAL("Tracker responded with failure message: %s\n", e->val->x.str.buf);
            return NULL;

        } else if (!strncmp(e->key.buf, "interval", (size_t)e->key.len)) {
            track->interval = e->val->x.num;

        } else if (!strncmp(e->key.buf, "tracker id", (size_t)e->key.len)) {
            track->id = (char*)calloc(e->val->x.str.len, sizeof(char));
            if (track->id == NULL) {
                perror("calloc");
                return NULL;
            }
            track->id = strncpy(track->id, e->val->x.str.buf, e->val->x.str.len);
            if (track->id == NULL) {
                perror("strncpy");
                return NULL;
            }

        } else if (!strncmp(e->key.buf, "complete", (size_t)e->key.len)) {
            track->complete = e->val->x.num;

        } else if (!strncmp(e->key.buf, "incomplete", (size_t)e->key.len)) {
            track->leechers = e->val->x.num;

        } else if (!strncmp(e->key.buf, "peers", (size_t)e->key.len)) {
            /* Now build a linked list of peer structures */
            peers_t *node = NULL;
            peers_t *prev = NULL;

            list_for_each(peers_position, &e->val->x.dict_head) {
                se = list_entry(peers_position, be_dict_t, link);

                if ((node = (peers_t*)calloc(1, sizeof(peers_t))) == NULL) {
                    perror("calloc");
                    return NULL;
                }

                if (!strncmp(se->key.buf, "peer id", se->key.len)) {
                    node->id = (char*)calloc(se->val->x.str.len, sizeof(char));
                    if (node->id == NULL) {
                        perror("calloc");
                        return NULL;
                    }
                    node->id = strncpy(node->id, e->val->x.str.buf, e->val->x.str.len);
                    if (node->id == NULL) {
                        perror("calloc");
                        return NULL;
                    }

                } else if (!strncmp(se->key.buf, "ip", se->key.len)) {
                    node->ip = (char*)calloc(se->val->x.str.len, sizeof(char));
                    if (node->ip == NULL) {
                        perror("calloc");
                        return NULL;
                    }
                    node->ip = strncpy(node->ip, e->val->x.str.buf, e->val->x.str.len);
                    if (node->ip == NULL) {
                        perror("calloc");
                        return NULL;
                    }

                } else if (!strncmp(se->key.buf, "port", se->key.len)) {
                    node->port = (char*)calloc(se->val->x.str.len, sizeof(char));
                    if (node->port == NULL) {
                        perror("calloc");
                        return NULL;
                    }
                    node->port = strncpy(node->port, e->val->x.str.buf, e->val->x.str.len);
                    if (node->port == NULL) {
                        perror("calloc");
                        return NULL;
                    }
                }

                /* Append the current node to the linked list of peers */
                if (prev == NULL) {
                    track->peers = node;
                    prev = node;
                } else {
                    prev->next = node;
                    prev = node;
                }
            }
            be_dict_free(se);

        } else if (!strncmp(e->key.buf, "warning message", (size_t)e->key.len)) {
            be_free(e->val);
            continue;

        } else if (!strncmp(e->key.buf, "min interval", (size_t)e->key.len)) {
            be_free(e->val);
            continue;
        }
    }
    be_dict_free(e);
    return track;
}

/**
 * Send an initial request to the tracker and print the response.
 * This is still very much in the prototyping stage, I'll switch to a
 * more meaningful return type later.
 */
int
tracker_request_peers(torrent_t *t)
{
    CURLcode   err;
    CURL *     curl    = NULL;
    char *     body    = NULL;
    char *     url     = NULL;
    if (t == NULL) return 1;

    /* Build the tracker API url */
    if ((url = gen_api_url(t)) == NULL) {
        perror("gen_api_url");
        return 1;
    }

    /* Create a buffer to store curl's response in */
    if ((body = (char*)calloc(CURL_MAX_WRITE_SIZE, sizeof(char))) == NULL) {
        perror("calloc");
        return 1;
    }
    
    /* Set up and send the request */
    if ((curl = curl_easy_init()) != NULL) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefunc_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)body);
        if ((err = curl_easy_perform(curl)) != CURLE_OK) {
            FATAL("CURL failed to reach tracker API: %s\n",
                  curl_easy_strerror(err));
        }
        curl_easy_cleanup(curl);
    } else {
        perror("curl_easy_init");
        return 1;
    }

    free(body);

    return 0;
}
