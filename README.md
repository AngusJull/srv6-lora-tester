# SRv6 Lora Tester

This program is designed for collecting and displaying different statistics related to using SRv6 over LoRa radios

The example is designed to launch into a shell, as well as display information of the esp32-heltec-lora32-v3 OLED display
included.

The statistics that are included so far are battery (in millivolts), statistics about tx/rx bytes and packets on the sx126x connected as an IEEE 802.15.4 device, and a small amount of information on recently sent/received IPV6 packets

## Usage

You'll first need to export the environment variable RIOTBASE so the build system knows how to build this project. Use a command like `export RIOTBASE=~/<some path>/RIOT`. Then, choose the port that RIOT should use to flash the board with `export PORT=/dev/<your port>`. Finally, you'll need to install the development tools for the ESP32, following RIOT's instructions at [this link](https://api.riot-os.org/group__cpu__esp32.html#esp32_local_toolchain_installation).

Make sure the ESP32 toolchain is activated, and also make sure you have pyserial installed (RIOT needs this to open the serial connection to the board after flashing).

Build, flash and start the application:

```
make
make flash
make term
```

Later, the ability to dump the stored information as JSON output will be added
