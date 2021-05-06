/* 
 * This file contains everything we need in order to populate a torrent
 * structure from a magnet URI passed on the command line.
 */

#include <endian.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <curl/curl.h>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "magnet.h"

#include "bencode/bencode.h"
#include "bencode/list.h"



/************* S M A L L   H E L P E R   F U N C T I O N S *************/



/**
 * Base conversion is fiddly and error-prone so I borrowed this from here:
 * github.com/transmission/transmission/blob/master/libtransmission/utils.c#L815
 */
static void
hex_to_binary(void const* vinput, void* voutput, size_t byte_length)
{
    static char const hex[] = "0123456789abcdef";

    uint8_t const* input = (uint8_t const*)vinput;
    uint8_t* output = voutput;

    for (size_t i = 0; i < byte_length; ++i) {
        int const hi = strchr(hex, tolower(*input++)) - hex;
        int const lo = strchr(hex, tolower(*input++)) - hex;
        *output++ = (uint8_t)((hi << 4) | lo);
    }
}

/**
 * These functions craft a UDP packet to send to the tracker as per bep 15:
 * www.bittorrent.org/beps/bep_0015.html
 * The first creates a connection (handshake) packet and the second creates
 * a packet to request peer information.
 * Take the url of the host we're contacting and a torrent structure, return
 * a successfully crafted packet or NULL on error
 */
static uint8_t *
udp_gen_conn_pkt(torrent_t *t)
{
    uint64_t magic_number   = htobe64(0x41727101980);
    uint32_t transaction_id = htobe32(3141592653);
    uint8_t *packet = (uint8_t*)malloc(17);
    memset(packet, 0, 17);

    if (packet == NULL) {
        perror("malloc");
        return NULL;
    }

    *packet        = (uint8_t)(magic_number >> 56);
    *(packet + 1)  = (uint8_t)((magic_number << 8) >> 56);
    *(packet + 2)  = (uint8_t)((magic_number << 16) >> 56);
    *(packet + 3)  = (uint8_t)((magic_number << 24) >> 56);
    *(packet + 4)  = (uint8_t)((magic_number << 32) >> 56);
    *(packet + 5)  = (uint8_t)((magic_number << 40) >> 56);
    *(packet + 6)  = (uint8_t)((magic_number << 48) >> 56);
    *(packet + 7)  = (uint8_t)((magic_number << 56) >> 56);
    *(packet + 8)  = 0;
    *(packet + 9)  = 0;
    *(packet + 10) = 0;
    *(packet + 11) = 0;
    *(packet + 12) = (uint8_t)(transaction_id >> 24);
    *(packet + 13) = (uint8_t)((transaction_id << 8) >> 24);
    *(packet + 14) = (uint8_t)((transaction_id << 16) >> 24);
    *(packet + 15) = (uint8_t)((transaction_id << 24) >> 24);

    t->trans_id = transaction_id;
    
    return packet;
}

static uint8_t *
udp_gen_annc_pkt(torrent_t *t)
{
    /* uint64_t connection_id = t->conn_id; */
    /* uint32_t transaction_id = t->trans_id; */
    /* etc */
    uint8_t *packet = (uint8_t*)malloc(99);
    memset(packet, 0, 99);

    if (t == NULL) return NULL;

    return packet;
}

/**
 * Parse a UDP tracker's response or the connect and announce sorts, 
 * respectively. The announce function fills in the torrent structure.
 */
int
udp_parse_connect(char *body)
{
    DEBUG("udp_parse_connect: body: %s\n", body);
    return 0;
}

int
udp_parse_announce(torrent_t *t, char *body)
{
    if (t == NULL) return -1;
    DEBUG("udp_parse_announce: body: %s\n", body);
    return 0;
}


/**
 * See CURLOPT_WRITEFUNCTION(3)
 */
static size_t
write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    memcpy(userp, data, size * nmemb);
    return size * nmemb;
}



/************* L A R G E   H E L P E R   F U N C T I O N S *************/



/**
 * Send an initial request to a tracker using the udp:// URI scheme.
 * Networking code adapted from here:
 * www.beej.us/guide/bgnet/html/index-wide.html#datagram
 */
static char *
udp_request(torrent_t *t, char *url, char *pkt)
{
    char *body = NULL;
    
    int sock, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;

    /* Initial network setup */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* IP version agnostic */
    hints.ai_socktype = SOCK_DGRAM; /* UDP socket */

    /* Get information about connections */
    char *tmp = strdup(url+6);
    char *host = strndup(tmp, (size_t)(strchr(tmp, ':') - tmp));
    if ((rv = getaddrinfo(host, t->port, &hints, &servinfo)) != 0) {
        FATAL("getaddrinfo: %s\n", gai_strerror(rv));
        DEBUG("URL: %s\n", url);
        free(host);
        free(tmp);
        return NULL;
    }
    free(host);
    free(tmp);

    /* Establish a socket */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        FATAL("Failed to create a UDP socket\n");
        return NULL;
    }

    /* Allocate an over-sized buffer for the tracker's response */
    if ((body = (char*)calloc(CURL_MAX_WRITE_SIZE + 1, sizeof(char))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Set a timeout for the socket */
    int n = 0;
    struct timeval timeout;
    timeout.tv_sec = 15;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout,
                   sizeof(timeout)) < 0) {
        perror("setsockopt");
        return NULL;
    }

    /* Loop forever until we receive a good response */
    size_t bytes;
    while (1) {
        /* Send the packet */
        if (sendto(sock, pkt, strlen(pkt), 0, p->ai_addr, p->ai_addrlen) == -1) {
            perror("sendto");
            return NULL;
        }

        /* Recieve a response */
        bytes = recvfrom(sock, body, CURL_MAX_WRITE_SIZE, 0,
                         (struct sockaddr *)&their_addr, &addr_len);

        /* If we've timeout out before, increase the timeout and try again */
        if (bytes == 0) {
            if (n < 8) {
                timeout.tv_sec = 15 * (int)pow(2, n);
                if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout,
                               sizeof(timeout)) < 0) {
                    perror("setsockopt");
                    return NULL;
                }
            } else {
                fprintf(stderr, "UDP connection to %s timed out\n", url);
                close(sock);
                free(pkt);
                freeaddrinfo(servinfo);
                return NULL;
            }
            n++;
        } else {
            break;
        }
    }

    /* Resize the body buffer */
    if ((body = (char*)reallocarray(body, bytes + 1, sizeof(char))) == NULL) {
        perror("reallocarray");
        return NULL;
    }
    char *end = body + bytes + 1;
    *end = '\0';

    close(sock);
    free(pkt);
    freeaddrinfo(servinfo);

    return body;
}

/**
 * Extract the tracker's bencoded response in BODY and use it to populate
 * the torrent structure T or report a failure condition. Return 0 iff
 * everything went well.
 */
static int
extract_tracker_bencode(torrent_t *t, char *body)
{
    be_node_t *bencode = NULL;
    be_dict_t *entry, *peer;
    list_t *   top_pos, *top_tmp;
    size_t     read;

    if (t == NULL || body == NULL) return -1;

    /* Convert the body into a bencode dictionary */
    if ((bencode = be_decode(body, strlen(body), &read)) == NULL) {
        perror("be_decode");
        return -1;
    }

    if (bencode->type != DICT) {
        FATAL("The tracker didn't return a dictionary");
        return -1;
    }

    /* Iterate over the dictionary using macros defined in bencode/list.h */
    list_for_each_safe(top_pos, top_tmp, &bencode->x.dict_head) {
        entry = list_entry(top_pos, be_dict_t, link);

        /* TODO: there are other fields I should support */
        if (!strcmp(entry->key.buf, "failure reason")) {
            FATAL("Tracker responded with failure: %s\n", entry->val->x.str.buf);

        } else if (!strcmp(entry->key.buf, "complete")) {
            /* Number of peers with the complete file */
            continue;

        } else if (!strcmp(entry->key.buf, "incomplete")) {
            /* Number of leachers */
            continue;

        } else if (!strcmp(entry->key.buf, "interval")) {
            /* Number of seconds to wait before sending another request */
            continue;

        } else if (!strcmp(entry->key.buf, "peers")) {
            /* This is where we build the linked list of peers */
            peers_t *curr, *prev;
            list_t  *peer_pos, *peer_tmp;

            list_for_each_safe(peer_pos, peer_tmp, &entry->val->x.dict_head) {
                peer = list_entry(peer_pos, be_dict_t, link);

                if ((curr = (peers_t*)calloc(1, sizeof(peers_t))) == NULL) {
                    perror("calloc");
                    return -1;
                }

                /* SEGFAULT because apparently peer->key.buf = 0x3 ???????? */
                if (!strcmp("peer id", peer->key.buf)) {
                    if ((curr->id = strdup(peer->val->x.str.buf)) == NULL) {
                        perror("strdup");
                        return -1;
                    }

                } else if (!strcmp("ip", peer->key.buf)) {
                    if ((curr->ip = strdup(peer->val->x.str.buf)) == NULL) {
                        perror("strdup");
                        return -1;
                    }

                } else if (!strcmp("port", peer->key.buf)) {
                    if ((curr->port = strdup(peer->val->x.str.buf)) == NULL) {
                        perror("strdup");
                        return -1;
                    }
                }
                /* Append this peer to the torrent structure's linked list */
                if (prev == NULL) {
                    t->peers = curr;
                    prev = curr;
                } else {
                    prev->next = curr;
                    prev = curr;
                }
            }
            DEBUG("AFTER PEER LOOP\n");
            be_dict_free(peer);
        }
        be_dict_free(entry);
    }

    return 0;
}



/***************** M A I N   A P I   F U N C T I O N S *****************/



/**
 * Send an initial request to a tracker. Returns 0 iff we successfully
 * received and parsed peer and chunk information.
 */
int
magnet_request_tracker(torrent_t *t)
{
    char *body = NULL;

    if (t == NULL) return -1;
    /* Try each of the announce urls until one works */
    for (tracker_t *a = t->trackers; a != NULL; a = a->next) {
        if (!strncmp(a->url, "udp", 3)) {
            uint8_t *connect_pkt, *announce_pkt;

            /* Generate the handshake packet */
            if ((connect_pkt = udp_gen_conn_pkt(t)) == NULL) {
                FATAL("Failed to generate the connection UDP packet\n");
                return -1;
            }

            /* Send and parse the intial handshake */
            if ((body = udp_request(t, a->url, (char*)connect_pkt)) == NULL) {
                free(connect_pkt);
                continue;
            }
            free(connect_pkt);
            if (udp_parse_connect(body) < 0) {
                free(body);
                free(connect_pkt);
                continue;
            }
            free(body);

            /* Generate the announce packet */
            if ((announce_pkt = udp_gen_annc_pkt(t)) == NULL) {
                FATAL("Failed to generate the announce UDP packet\n");
                return -1;
            }

            /* Send the announce and parse the response into T */
            if ((body = udp_request(t, a->url, (char*)announce_pkt)) == NULL) {
                free(announce_pkt);
                continue;
            }
            free(announce_pkt);
            if (udp_parse_announce(t, body) < 0) {
                free(announce_pkt);
                free(body);
                continue;
            }
            
            free(body);
            break;

        } else if (!strncmp(a->url, "http", 4)) {
            CURLcode err;
            CURL *curl = NULL;
            char url[1024];

            /* Allocate a buffer to store curl's response */
            if ((body = (char*)calloc(CURL_MAX_WRITE_SIZE, sizeof(char))) == NULL) {
                perror("calloc");
                return -1;
            }

            /* Create a tracker API url */
            memset(url, 0, 1024);
            sprintf(url, "%s?info_hash=%s&peer_id=%s&port=%s&uploaded=%lli&downloaded=%lli&left=%lli&compact=%s&event=%s",
                    a->url, t->info_hash, t->peer_id, t->port, t->uploaded,
                    t->dloaded, t->left, "0", t->event);

            if ((curl = curl_easy_init()) != NULL) {
                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)body);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
                err = curl_easy_perform(curl);
                if (err == CURLE_OPERATION_TIMEDOUT) {
                    fprintf(stderr, "CURL timed out for %s\n", a->url);
                    continue;
                }
                if (err) {
                    FATAL("CURL failed to reach tracker API: %s\n",
                          curl_easy_strerror(err));
                    return -1;
                }
                curl_easy_cleanup(curl);
                break;
            }
        }
    }

    DEBUG("BODY: (%s)\n", body);
    if (*body == '\0') {
        FATAL("None of the trackers responded :(\n");
        return -1;
    }

    /* Decode the tracker's response */
    if (extract_tracker_bencode(t, body) < 0) {
        FATAL("Error occurred while parsing the request body\n");
        return -1;
    }

    free(body);
    return 0;
}

/**
 * Take a magnet link and parse its contents into the torrent structure
 */
torrent_t *
magnet_parse_uri(char *magnet)
{
    torrent_t *t     = NULL;
    tracker_t *curr  = NULL;
    tracker_t *prev  = NULL;
    char      *token = NULL;

    if (magnet == NULL) return NULL;

    /* Allocate the torrent struct */
    if ((t = (torrent_t*)calloc(1, sizeof(torrent_t))) == NULL) {
        perror("calloc");
        return NULL;
    }

    /* Increment the magnet pointer to the first '?' */
    if ((magnet = strchr(magnet, '?') + 1) == NULL) {
        FATAL("Invalid magnet URL\n");
        return NULL;
    }

    /* Extract the key-value pairs from the magnet */
    char *div = NULL;
    while ((div = strchr(magnet, '&')) != NULL || strrchr(magnet, '&') == NULL) {
        if ((token = strndup(magnet, (size_t)(div - magnet))) == NULL) {
            perror("strndup");
            return NULL;
        }

        if (!strncmp("xt", token, 2)) {        /* File hash */
            char *hex, bin[100], *binptr = &bin[0];
            memset(bin, 0, 100);

            if ((hex = strdup(strrchr(token, ':') + 1)) == NULL) {
                perror("strdup");
                return NULL;
            }

            /* Hex is just too pretty, so we need URL-encoded binary :/ */
            hex_to_binary(hex, binptr, 20);
            CURL *curl = curl_easy_init();
            if (curl) {
                t->info_hash = curl_easy_escape(curl, binptr, strlen(binptr));
                if (t->info_hash == NULL) {
                    perror("curl_easy_unescape");
                    return NULL;
                }
                curl_easy_cleanup(curl);
            }

            free(hex);
            
        } else if (!strncmp("dn", token, 2)) { /* Torrent name */
            if ((t->filename = strdup(token + 3)) == NULL) {
                perror("strdup");
                return NULL;
            }
            
        } else if (!strncmp("tr", token, 2)) { /* URLencoded tracker URL */
            if ((curr = (tracker_t*)calloc(1, sizeof(tracker_t))) == NULL) {
                perror("calloc");
                return NULL;
            }

            /* Decode the URL */
            CURL *curl  = curl_easy_init();
            if (curl) {
                curr->url = curl_easy_unescape(curl, token+3, strlen(token)-2, NULL);
                if (curr->url == NULL) {
                    perror("curl_easy_unescape");
                    return NULL;
                }
                curl_easy_cleanup(curl);
            }

            /* Append to the announce linked list */
            if (prev == NULL) {
                t->trackers = curr;
                prev = curr;
            } else {
                prev->next = curr;
                prev = curr;
            }
        }
        free(token);
        magnet = div + 1;
        if (div == NULL) break;
    } 

    if (t->info_hash == NULL) {
        FATAL("Mangled magnet, failed to extract info hash\n");
        return NULL;
    }

    if (t->trackers == NULL) {
        FATAL("Mangled magnet, failed to extract trackers\n");
        return NULL;
    }

    if (t->filename == NULL) {
        FATAL("Mangled magnet, failed to extract filename\n");
        return NULL;
    }

    return t;
}
