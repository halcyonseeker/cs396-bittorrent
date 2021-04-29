/* 
 * extract.h --- the metainfo parsing logic is big and ugly, let's hide it
 */

#pragma once

#include "bitclient.h"

extern int extract_from_bencode(torrent_t *t);
