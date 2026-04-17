# JACK-Warden

JACK-Warden is a lightweight CLI-based daemon for managing JACK audio connections.

It monitors system events (via UDEV) and automatically connects or disconnects JACK devices based on predefined rules. This makes it easier to handle dynamic audio setups, especially with USB sound cards.

## Features

* Automatic device detection using UDEV
* Auto connect / disconnect for JACK devices
* Simple configuration via `cards.conf`
* Designed for CLI workflows and minimal environments

## How It Works

JACK-Warden listens to UDEV events and matches connected devices against entries defined in:

```
~/.config/jack-warden/cards.conf
```

When a known device is detected, it binds the appropriate JACK ports. When the device is removed, it safely disconnects it.

## Configuration

Each device is defined using:

```
system: <UDEV ID (e.g. ID_USB_MODEL)>
device: <ALSA device name>
```

## Running

```
jack-warden --config-file ~/.config/jack-warden/cards.conf
```

## Notes

* Requires a running JACK server
* Uses ALSA device names from `/proc/asound/cards`
* Relies on UDEV for hotplug detection
