/*
 * tracker.c -- Request information from and update trackers
 */

#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "bitclient.h"
#include "tracker.h"

/**
 * Send an initial request to the tracker and print the response.
 * This is still very much in the prototyping stage, I'll switch to a
 * more meaningful return type later.
 */
int
tracker_request_peers(torrent_t *t)
{
    if (t == NULL) return 1;

    return 0;
}
