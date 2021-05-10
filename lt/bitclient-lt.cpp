//
// bitclient.cpp --- A last-minute attempt at "implementing" a BitTorrent
// client in C++ using libtorrent.
//
// Thalia Wright <wrightng@reed.edu>
//

#include <iostream>

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>

static bool log_verbosely = true;

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: ./bitclient-lt [magnet]" << std::endl;
        return -1;
    }

    // Register a few settings regarding verbosity
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::alert_mask,
                     lt::alert_category::error   |
                     lt::alert_category::storage |
                     lt::alert_category::status);

    // Add the magnet URL
    lt::add_torrent_params params = lt::parse_magnet_uri(argv[1]);
    params.save_path = ".";

    // Create the torrent session with the previous settings
    lt::session session(settings);

    // Control the torrent
    lt::torrent_handle torrent = session.add_torrent(std::move(params));

    std::cout << "Downloading " << params.name << "..." << std::endl;

    // New enter a loop, polling the library for info until we're done
    for (;;) {
        std::vector<lt::alert*> alerts;
        session.pop_alerts(&alerts);

        for (lt::alert const *a : alerts) {
            if (log_verbosely)
                std::cout << a->message() << std::endl;

            if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
                std::cout << "Torrent finished!" << std::endl;
                session.abort(); // Remove these to keep seeding
                return 0;        //
            } else if (lt::alert_cast<lt::torrent_error_alert>(a)) {
                std::cerr << "Failed to download the torrent :(" << std::endl;
                return -1;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
    return 0;
}
