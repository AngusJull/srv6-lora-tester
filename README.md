# SRv6 Lora Tester

This program is designed for collecting and displaying different statistics
related to using SRv6 over LoRa radios

The example is designed to launch into a shell, as well as display information
of the esp32-heltec-lora32-v3 OLED display included.

The statistics that are included so far are battery (in millivolts), statistics
about tx/rx bytes and packets on the sx126x connected as an IEEE 802.15.4
device, and a small amount of information on recently sent/received IPV6 packets

## Usage of the Application

You'll first need to export the environment variable RIOTBASE so the build
system knows how to build this project. Use a command like
`export RIOTBASE=~/<some path>/RIOT`. Then, choose the port that RIOT should use
to flash the board with `export PORT=/dev/<your port>`. Finally, you'll need to
install the development tools for the ESP32, following RIOT's instructions
[on their forum](https://api.riot-os.org/group__cpu__esp32.html#esp32_local_toolchain_installation).

You'll specifically a forked version of the RIOT operating system which includes
work to integrate the sx1262 radio with the IEEE 802.15.4 hardware abstraction
layer. This forked version also includes a way for the program to pick up data
before it is sent to the network interface (making this program incompatible
with the original). The forked version can be found [on GitHub](https://github.com/AngusJull/RIOT).

Make sure the ESP32 toolchain is activated, and also make sure you have pyserial
installed (RIOT needs this to open the serial connection to the board after flashing).

Build, flash and start the application:

```
make
make flash
make term
```

You can then interact with the program through the shell, which runs by default.
For example, use the command `ps` to see what processes (including network
interfaces) are running.

The collected data can be displayed using the command `dump_buffer`, which will display all the information in a JSON format.

## Network Configurations

The program includes configuration that runs to set up IPv6 addresses, IEEE 802.15.4 addresses, 6LoWPAN compression context,
and the forwarding entires. Configurations for the network of boards are stored in topologies, and each board has an ID in
that topology. These can be set at build time, or after flashing. To set these during compile time, use the environment variables
`CONFIG_ID`, `TOPOLOGY_ID` and `USE_SRV6`.

To update a board's configuration after flashing the board, use the command `set_config [id <node id>]  [topo <topology id>] [sr <use srv6>] [tp <use throughput test>]`,
where the first two arguments are IDs (you can see the behaviour of the different topologies
in the `src/configs` folder), and the last two arguments are set to 0 or 1 to tell the boards to send traffic using SRv6 headers and to use large packets, respectively.

## Gathering and Interpreting Data

This application is designed to collect data related to the use of SRv6 over LoRa radios. As such, there are also some tools included to make collecting this data and turning it into useful information under the `tools/` directory. These tools are written in Python.

Before using the tools, install their dependencies using pip in a virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

### Recording Data

You can then run the `tools/recorder.py` script to collect data from the boards and save it in files. Provide the port to use and optionally a filename to save the results under. You should have the first board plugged in when the script is run, and then subsequent boards can have their information collected as is prompted by the program. The script is designed to prevent data loss and saves the recorded information back to the disk every time it is collected.

The recorder also allows you to change the configuration on the boards as their data is being collected - the boards don't update their configuration until rebooted so this allows reconfiguration to be done faster.

### Analyzing Data

Once the data has been recorded, it can be processed by the `tools/analysis.py` script. This one simply crunches some numbers and prints statistics of interest that are easier to compare. Provide it the filename that the `tools/recorder.py` saved the data under.

You can also interact with the data as Polars dataframes (similar to Pandas dataframes) by running a Python interpreter (or `ipython` for a better experience) and using the functions in `tools/analysis.py` to load the data into dataframes. This allows you to perform your own analysis on the data.

## Future Work

There are some remaining features and issues to be sorted out

- Use real payloads for sent and received packets (CoAP for example)
- Improve display on the boards - make more descriptive
- Further explain the work related to this project and its results

This project was completed without any AI writing tools
