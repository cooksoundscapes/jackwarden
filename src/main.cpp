
#include <csignal>
#include <atomic>
#include <CLI/CLI.hpp>
#include "jack_handler.h"

using namespace std;

atomic<bool> shouldQuit = false;

static void terminateSigHandler(int) {
    shouldQuit = true;
}

int main(int argc, char** argv)
{
    signal(SIGINT, terminateSigHandler);
    signal(SIGTERM, terminateSigHandler);

    CLI::App app{"JACK-Warden: A resilient JACK server binding"};

    auto home = getenv("HOME");
    string configPath = home ? string(home) + "/.config/jack-warden/cards.conf" : "";

    app.add_option(
        "-c, --config-file", 
        configPath, 
        "Configuration file for sound cards"
    )->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    JackHandler jackHandler(configPath);

    jackHandler.watch(shouldQuit);

    return 0;
}
