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
