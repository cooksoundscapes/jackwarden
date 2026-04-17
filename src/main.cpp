
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

    app.footer(R"(Registering new sound cards:

1. Monitor udev events and plug your device:
   udevadm monitor --environment | grep ID_USB_MODEL=

   Copy the value of ID_USB_MODEL.

2. Add it to your config (~/.config/jack-warden/cards.conf):
   Use the copied value as the "system:" field.

3. Find the ALSA device name:
   cat /proc/asound/cards

   Identify your device and copy its name.

4. In cards.conf:
   Use that value as the "device:" field.

Example:

[card]
system: MY_USB_DEVICE
device: DeviceName

This allows jack-warden to bind the correct JACK device.)");

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
