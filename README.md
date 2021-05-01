Bitclient: A Simple BitTorrent Client
=====================================

Build Instructions
==================
1. Download [this bencode library](https://github.com/cwyang/bencode):
   ```
       git submodule init
       cd bencode; make
   ```

2. Install [libcurl](https://curl.se/libcurl/) if you don't have it
   already.
   
3. To build the client, just run `make`

Design
======
## Known Issues

## Future Expansions

Useful Links
============
 * https://en.wikipedia.org/wiki/BitTorrent
 * https://wiki.theory.org/BitTorrentSpecification

 * http://www.bittorrent.org/beps/bep_0000.html
 * http://www.bittorrent.org/beps/bep_0003.html
 * http://www.bittorrent.org/beps/bep_0052.html

 * http://dandylife.net/docs/BitTorrent-Protocol.pdf
 * https://www.morehawes.co.uk/the-bittorrent-protocol
 * https://www.beautifulcode.co/blog/58-understanding-bittorrent-protocol
 * https://skerritt.blog/bit-torrent/
 * https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html
