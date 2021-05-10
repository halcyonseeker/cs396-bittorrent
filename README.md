Bitclient: A Simple BitTorrent Client
=====================================

This repository contains two attempts at my final project for CS386
(Computer Networks) at Reed College.

- v1-broken/ contains a mostly non-functional BitTorrent client
  written (mostly) from scratch in C.
- v2-working/ contains a fully-functional BitTorrent client written in
  C++ that offloads all the interesting parts to LibTorrent.

The Broken C Version
====================
This version lives in the v1-broken/ directory.

1. Download and build 
   [this bencode library](https://github.com/cwyang/bencode). 
   (`git submodule init; cd bencode; make`).
2. In the unlikely event that it isn't pre-installed, install
   [libcurl](https://curl.se/libcurl/).
3. To build the program just run `make`.

My initial plan was to use C to write a program that would take a
.torrent file, decode it, ask one of the specified trackers for a list
of peers, and then download the file or files from them. I got it to
the point where I was able to send a HTTP request to a tracker, but
they all responded with errors along the lines of "torrent not
valid". Turns out that the bencode library I was using (and another I
tired), were unable to re-encode the info dictionary into bencode that
would produce a valid checksum.

At this point I gave up on metainfo files and rewrote it to use the
more modern magnet URI scheme. Unfortunately, all the magnet URIs I
found preferred UDP trackers. I attempted to write a client for the
UDP tracker protocol, but was unable to figure out how to pack
together a valid packet.

Therefore, in order to actually get a list of peers, we need a
hand-crafted magnet URI that uses HTTP trackers. At this point I
was running rather short on time and chose to abandon this program
in favour of a LibTorrent wrapper discussed below. The code in the
old project is organised in the v1-broken/ directory as follows:

- **bitclient.c**   Contains the programs main; its responsibility is
  to build the `torrent_t` structure with information about the file
  we want to download, then spawning off a pair of threads to
  concurrently download and upload chunks to and from peers.
- **bitclient.h**   Contains a few macros and definition of the
  central torrent structure.
- **magnet.(c,h)**  Exposes a pair of functions to main, the first of
  which parses the magnet URI and the second of which uses that
  information to contact trackers.
- **leecher.(c,h)** Exposes the function to main which is responsible
  for downloading the file from peers. 
- **seeder.(c,h)**  Exposes the function to main which uploads pieces
  of the file to peers.

The bak/ directory also contains **extract.(c,h)** and
**tracker.(c,h)**, which, in the earlier iteration of the program,
were responsible for decoding the metainfo file and requesting
information from a HTTP tracker.

The existing program takes two flags, one to print a help message and
exit (`-h`), and one to print debugging information (`-v`). I strongly
recommend running it with the latter flag. For the sake of simplicity
I only chose to support downloading one torrent at a time, concurrency
can be achieved with an external tool like `xargs(1)`.

The Boring But Working LibTorrent Version
=========================================

The source and Makefile live in v2-working/.
  
1. Install [LibTorrent](https://libtorrent.org). Weirdly, on Arch
   GNU/Linux the libtorrent package didn't contain the C++ header
   files, so I had to install the libtorrent-rasterbar package. I
   believe it depends on Boost.
2. Just run `make`.

Upon realising that I was on a trajectory to not complete this
assignment in time, I investigated LibTorrent, a BitTorrent client
implementation in a C++ library, and threw together a basic program.

I was initially resistant to using any kind of library because my
primary goal was to learn how BitTorrent, and peer-to-peer protocols
more generally, work. I believe I achieved this goal while trying to
write the first version. Although I didn't gain any knowledge writing
second version (other than how to use LibTorrent and a reminder of how
much I dislike C++), I produced a program that could successfully
download /ahem/ Linux ISOs from magnet links.

This program lives in a single C++ file and doesn't take any options
other than a magnet link. It does, however, contain a `log_verbosely`
boolean which you can set to `true` if you want to see something more
interesting that the number of bytes downloaded. By default I also
have it set to exit when the torrent is fully downloaded, thought this
can be changed by commenting out the marked lines.

Useful Links
============
- https://en.wikipedia.org/wiki/BitTorrent
- https://wiki.theory.org/BitTorrentSpecification

- http://www.bittorrent.org/beps/bep_0000.html
- http://www.bittorrent.org/beps/bep_0003.html
- http://www.bittorrent.org/beps/bep_0052.html

- http://dandylife.net/docs/BitTorrent-Protocol.pdf
- https://www.morehawes.co.uk/the-bittorrent-protocol
- https://www.beautifulcode.co/blog/58-understanding-bittorrent-protocol
- https://skerritt.blog/bit-torrent/
- https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html

- http://libtorrent.org/
