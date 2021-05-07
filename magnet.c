/* 
 * This file contains everything we need in order to populate a torrent
 * structure from a magnet URI passed on the command line.
 */

#include <errno.h>
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
 * Ansi C "itoa" based on Kernighan & Ritchie's "Ansi C":
 * Sourced from http://www.strudel.org.uk/itoa/
 * These are used for debugging in the packet generating functions
 */

static void
_strreverse(char* begin, char* end) {
    char aux;
    while(end>begin)
        aux=*end, *end--=*begin, *begin++=aux;
}

static void
_itoa(long long value, char* str, int base) {
    static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char* wstr=str;
    long long sign;

    // Validate base
    if (base<2 || base>35){ *wstr='\0'; return; }

    // Take care of sign
    if ((sign=value) < 0) value = -value;

    // Conversion. Number is reversed.
    do *wstr++ = num[value%base]; while(value/=base);

    if(sign<0) *wstr++='-';
    *wstr='\0';

    // Reverse string
    _strreverse(str,wstr-1);
}



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

    /* WARNING:
    * Because of timeout issues in udo_request, I have no idea if this packet
    * is actually valid. */

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

    /* Print the packet as binary */
    if (log_verbosely) {
        char dbg_magnum[65], dbg_transid[33], dbg_packet[137],
            *ptr = &dbg_packet[0];

        memset(dbg_magnum, 0, 65);
        memset(dbg_transid, 0, 33);
        memset(dbg_packet, 0, 137);

        _itoa(magic_number, dbg_magnum, 2);
        _itoa(transaction_id, dbg_transid, 2);
        for (int i = 0; i < 16; i++) { _itoa(packet[i], ptr, 2); ptr++; }

        printf("-- BEGIN PACKET --\n");
        printf("%s00000000000000000000000000000000%s\n", dbg_magnum, dbg_transid);
        printf("%s\n", dbg_packet);
        printf("--- END PACKET ---\n");
    }

    t->trans_id = transaction_id;
    
    return packet;
}

static uint8_t *
udp_gen_annc_pkt(torrent_t *t)
{
    uint64_t connection_id  = t->conn_id;           /* Set in udp_gen_annc_pkt */
    uint32_t action         = htobe32(1);
    uint32_t transaction_id = t->trans_id;          /* Same as above */
    char     info_hash[21];
    char     peer_id[21];
    uint64_t downloaded     = htobe64((uint64_t)t->dloaded);
    uint64_t left           = htobe64((uint64_t)t->left);
    uint64_t uploaded       = htobe64((uint64_t)t->uploaded);
    uint32_t event          = 0;
    uint8_t *packet = (uint8_t*)malloc(84);
    memset(&info_hash, 0, 20);
    memset(&peer_id, 0, 20);
    memset(packet, 0, 84);

    if (packet == NULL) {
        perror("malloc");
        return NULL;
    }

    /* WARNING:
    * Because of timeout issues in udo_request, I have no idea if this packet
    * is actually valid. */

    /* Copy info_hash and peer_id into big endian buffers */
    for (size_t i = 0; i < 20; i++) info_hash[i] = (char)htobe32(*(t->info_hash + i));
    for (size_t i = 0; i < 20; i++) peer_id[i] = (char)htobe32(*(t->peer_id + i));

    /* Set the event based on the current event */
    if      (!strcmp(t->event, "completed")) event = htobe32(1);
    else if (!strcmp(t->event, "started"))   event = htobe32(2);
    else if (!strcmp(t->event, "stopped"))   event = htobe32(3);

    *packet        = (uint8_t)(connection_id >> 56);
    *(packet + 1)  = (uint8_t)((connection_id << 8) >> 56);
    *(packet + 2)  = (uint8_t)((connection_id << 16) >> 56);
    *(packet + 3)  = (uint8_t)((connection_id << 24) >> 56);
    *(packet + 4)  = (uint8_t)((connection_id << 32) >> 56);
    *(packet + 5)  = (uint8_t)((connection_id << 40) >> 56);
    *(packet + 6)  = (uint8_t)((connection_id << 48) >> 56);
    *(packet + 7)  = (uint8_t)((connection_id << 56) >> 56);
    *(packet + 8)  = 0;
    *(packet + 9)  = 0;
    *(packet + 10) = 0;
    *(packet + 11) = (uint8_t)action;
    *(packet + 12) = (uint8_t)(transaction_id >> 24);
    *(packet + 13) = (uint8_t)((transaction_id << 8) >> 24);
    *(packet + 14) = (uint8_t)((transaction_id << 16) >> 24);
    *(packet + 15) = (uint8_t)((transaction_id << 24) >> 24);
    for (int i = 0, p = 16; i < 20; i++, p++) *(packet + p) = (uint8_t)info_hash[i];
    for (int i = 0, p = 36; i < 20; i++, p++) *(packet + p) = (uint8_t)peer_id[i];
    *(packet + 56) = (uint8_t)(downloaded >> 56);
    *(packet + 57) = (uint8_t)((downloaded << 8) >> 56);
    *(packet + 58) = (uint8_t)((downloaded << 16) >> 56);
    *(packet + 59) = (uint8_t)((downloaded << 24) >> 56);
    *(packet + 61) = (uint8_t)((downloaded << 32) >> 56);
    *(packet + 62) = (uint8_t)((downloaded << 40) >> 56);
    *(packet + 63) = (uint8_t)((downloaded << 48) >> 56);
    *(packet + 63) = (uint8_t)((downloaded << 56) >> 56);
    *(packet + 64) = (uint8_t)(left >> 56);
    *(packet + 65) = (uint8_t)((left << 8) >> 56);
    *(packet + 66) = (uint8_t)((left << 16) >> 56);
    *(packet + 67) = (uint8_t)((left << 24) >> 56);
    *(packet + 68) = (uint8_t)((left << 32) >> 56);
    *(packet + 69) = (uint8_t)((left << 40) >> 56);
    *(packet + 70) = (uint8_t)((left << 48) >> 56);
    *(packet + 71) = (uint8_t)((left << 56) >> 56);
    *(packet + 72) = (uint8_t)(uploaded >> 56);
    *(packet + 73) = (uint8_t)((uploaded << 8) >> 56);
    *(packet + 74) = (uint8_t)((uploaded << 16) >> 56);
    *(packet + 75) = (uint8_t)((uploaded << 24) >> 56);
    *(packet + 76) = (uint8_t)((uploaded << 32) >> 56);
    *(packet + 77) = (uint8_t)((uploaded << 40) >> 56);
    *(packet + 78) = (uint8_t)((uploaded << 48) >> 56);
    *(packet + 79) = (uint8_t)((uploaded << 56) >> 56);
    *(packet + 80) = (uint8_t)(event >> 24);
    *(packet + 81) = (uint8_t)((event << 8) >> 24);
    *(packet + 82) = (uint8_t)((event << 16) >> 24);
    *(packet + 83) = (uint8_t)((event << 24) >> 24);

    /* Print the packet as binary */
    if (log_verbosely) {
        char dbg_connid[65], dbg_action[33], dbg_transid[33], dbg_ihash[161],
            dbg_pid[161], dbg_dloaded[65], dbg_left[65], dbg_uploaded[65],
            dbg_event[33], dbg_packet[793], *ihash_ptr = &dbg_ihash[0],
            *pid_ptr = &dbg_pid[0], *packet_ptr = &dbg_packet[0];

        memset(dbg_connid, 0, 65);
        memset(dbg_action, 0, 33);
        memset(dbg_transid, 0, 33);
        memset(dbg_ihash, 0, 161);
        memset(dbg_pid, 0, 161);
        memset(dbg_dloaded, 0, 65);
        memset(dbg_left, 0, 65);
        memset(dbg_uploaded, 0, 65);
        memset(dbg_event, 0, 33);
        memset(dbg_packet, 0, 793);

        _itoa(connection_id, dbg_connid, 2);
        _itoa(action, dbg_action, 2);
        _itoa(transaction_id, dbg_transid, 2);
        for (int i = 0; i < 21; i++) { _itoa(packet[i], ihash_ptr, 2); ihash_ptr++; }
        for (int i = 0; i < 21; i++) { _itoa(packet[i], pid_ptr, 2); pid_ptr++; }
        _itoa(downloaded, dbg_dloaded, 2);
        _itoa(left, dbg_left, 2);
        _itoa(uploaded, dbg_uploaded, 2);
        _itoa(event, dbg_event, 2);
        for (int i = 0; i < 84; i++) { _itoa(packet[i], packet_ptr, 2); packet_ptr++; }

        printf("-- BEGIN PACKET --\n");
        printf("%s%s%s%s%s%s%s%s%s%s\n", dbg_connid, dbg_action, dbg_transid,
               dbg_ihash, dbg_pid, dbg_dloaded, dbg_left, dbg_uploaded,
               dbg_event, dbg_packet);
        printf("%s\n", dbg_packet);
        printf("--- END PACKET ---\n");
    }

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
udp_request(torrent_t *t, char *url, char *pkt, char *body)
{
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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
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

    /* Set up a timeout */
    int n = 0;
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        freeaddrinfo(servinfo);
        close(sock);
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
        if (errno == EWOULDBLOCK || body == 0) {   /* The socket timed out */
            if (n < 8) {
                DEBUG("UDP connection to %s timed out, retrying\n", url);
                tv.tv_sec = 15 * (int)pow(2, n);
                if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                    perror("setsockopt");
                    freeaddrinfo(servinfo);
                    close(sock);
                    return NULL;
                }
                n++;
                continue;
            } else {
                DEBUG("UDP timeout to %s exceeded the max threshold\n", url);
                close(sock);
                freeaddrinfo(servinfo);
                return NULL;
            }
        }
        break;
    }

    /* Resize the body buffer */
    if ((body = (char*)reallocarray(body, bytes + 1, sizeof(char))) == NULL) {
        perror("reallocarray");
        return NULL;
    }
    char *end = body + bytes + 1;
    *end = '\0';

    close(sock);
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
    be_dict_t *entry;
    list_t *   a_pos, *a_tmp;
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
    list_for_each_safe(a_pos, a_tmp, &bencode->x.dict_head) {
        entry = list_entry(a_pos, be_dict_t, link);

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
            if (entry->val->type == LIST) {
                list_t *b_pos, *b_tmp;
                be_node_t *plist;
                peers_t *curr = NULL, *prev = NULL;

                list_for_each_safe(b_pos, b_tmp, &entry->val->x.list_head) {
                    plist = list_entry(b_pos, be_node_t, link);

                    if (plist->type == DICT) {
                        list_t *c_pos, *c_tmp;
                        be_dict_t *pdict;

                        /* Allocate a node in the list */
                        if ((curr = (peers_t*)calloc(1, sizeof(peers_t))) == NULL) {
                            perror("calloc");
                            return -1;
                        }

                        list_for_each_safe(c_pos, c_tmp, &plist->x.dict_head) {
                            pdict = list_entry(c_pos, be_dict_t, link);

                            if (!strcmp("peer id", pdict->key.buf)) {
                                curr->id = strdup(pdict->val->x.str.buf);
                                if (curr->id == NULL) {
                                    perror("strdup");
                                    return -1;
                                }

                            } else if (!strcmp("ip", pdict->key.buf)) {
                                curr->ip = strdup(pdict->val->x.str.buf);
                                if (curr->ip == NULL) {
                                    perror("strdup");
                                    return -1;
                                }

                            } else if (!strcmp("port", pdict->key.buf)) {
                                char buf[20];
                                memset(buf, 0, 20);
                                sprintf(buf, "%lli", pdict->val->x.num);
                                if ((curr->port = strdup(buf)) == NULL) {
                                    perror("strdup");
                                    return -1;
                                }
                            }
                            be_dict_free(pdict);
                        }
                        /* Append this peer to the torrent's linked list */
                        if (prev == NULL) {
                            t->peers = curr;
                            prev = curr;
                        } else {
                            prev->next = curr;
                            prev = curr;
                        }

                    } else {
                        fprintf(stderr, "Unknown plist type; enum value: %i\n",
                              plist->type);
                        return -1;
                    }
                }
                be_free(plist);
            } else {
                if (entry->val->type == STR) {
                    if (entry->val->x.str.len == 0) {
                        FATAL("The tracker didn't send us any peers :(\n");
                        return -1;
                    }
                } else {
                    FATAL("Unknown entry type; enum value: %i\n",
                          entry->val->type);
                    return -1;
                }
            }
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

    /* Allocate a buffer to store curl's response */
    if ((body = (char*)calloc(CURL_MAX_WRITE_SIZE + 1, sizeof(char))) == NULL) {
        perror("calloc");
        return -1;
    }

    /* Try each of the announce urls until one works */
    for (tracker_t *a = t->trackers; a != NULL; a = a->next) {
        /* Make sure the body is properly zeroed */
        memset(body, 0, CURL_MAX_WRITE_SIZE + 1);

        if (!strncmp(a->url, "udp", 3)) {
            uint8_t *connect_pkt, *announce_pkt;

            /* Generate the handshake packet */
            if ((connect_pkt = udp_gen_conn_pkt(t)) == NULL) {
                FATAL("Failed to generate the connection UDP packet\n");
                return -1;
            }

            /* Send and parse the intial handshake */
            if (udp_request(t, a->url, (char*)connect_pkt, body) == NULL) {
                free(connect_pkt);
                continue;
            }
            free(connect_pkt);
            if (udp_parse_connect(body) < 0) {
                free(connect_pkt);
                continue;
            }

            /* Generate the announce packet */
            if ((announce_pkt = udp_gen_annc_pkt(t)) == NULL) {
                FATAL("Failed to generate the announce UDP packet\n");
                return -1;
            }

            /* Send the announce and parse the response into T */
            if (udp_request(t, a->url, (char*)announce_pkt, body) == NULL) {
                free(announce_pkt);
                continue;
            }
            free(announce_pkt);
            if (udp_parse_announce(t, body) < 0) {
                free(announce_pkt);
                continue;
            }
            break;

        } else if (!strncmp(a->url, "http", 4)) {
            CURL *curl = NULL;
            char url[1024];

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
                CURLcode err = curl_easy_perform(curl);
                if (err) {
                    fprintf(stderr, "CURL failed to reach tracker %s: %s\n",
                            a->url, curl_easy_strerror(err));
                    curl_easy_cleanup(curl);
                    continue;
                } else {
                    curl_easy_cleanup(curl);
                    break;
                }
            } else {
                FATAL("Failed to intialize curl\n");
            }
        } else {
            fprintf(stderr, "Url has unknown protocol scheme: %s\n", a->url);
            free(body);
            continue;
        }
    }

    DEBUG("BODY: (%s)\n", body);
    if (body == NULL) {
        FATAL("None of the trackers responded :(\n");
        return -1;
    } else if (*body == '\0') {
        FATAL("The trackers all responded with an empty request :(\n");
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
