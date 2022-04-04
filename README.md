# PingBoard

> Mini keyboard with just four keys for core macros.

## Building the firmware

### with Platform.io

Install platformio with

```bash
pip3 install platformio
```

Or pick any other installation method from https://docs.platformio.org/en/latest/core/installation.html

Review https://docs.platformio.org/en/latest//faq.html#platformio-udev-rules and install the udev rules.

To refresh your groups inside a running session without logging in, you can use:

```bash
su - $USER
```

After doing this (or a reboot), you can then flash the firmware with:

```bash
cd Prototype/PlatformioProjekt
platformio run -t upload
```

## License

This project is published under the MIT license, see [LICENSE](LICENSE.txt) for details.
