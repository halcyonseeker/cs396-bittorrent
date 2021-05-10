#pragma once

#include "bitclient.h"

extern torrent_t *magnet_parse_uri(char *magnet);
extern int magnet_request_tracker(torrent_t *t);
