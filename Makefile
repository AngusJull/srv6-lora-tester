APPLICATION = srv6-lora-tester

# If no BOARD is found in the environment, use this default:
BOARD ?= esp32-heltec-lora32-v3

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../..

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
# include and auto-initialize all available sensors
USEMODULE += saul_default

# netif is required for this application (though should still work if optional)
FEATURES_REQUIRED += netif
FEATURES_REQUIRED += periph_gpio
FEATURES_REQUIRED += periph_adc
FEATURES_REQUIRED += periph_i2c

# use modules for networking

# gnrc is a meta module including all required, basic gnrc networking modules
USEMODULE += gnrc
# use the default network interface for the board
USEMODULE += netdev_default
# auto init networking modules
USEMODULE += auto_init_gnrc_netif
# enable ipv6 networking stack, which automatically includes sixlowpan if a IEEE 802.15.4 device present
USEMODULE += gnrc_ipv6_router_default
# shell command to send L2 packets with a simple string
USEMODULE += gnrc_txtsnd
# the application dumps received packets to stdout
USEMODULE += gnrc_pktdump
# include udp
USEMODULE += gnrc_udp
#include test program for udp send/recv
USEMODULE += shell_cmd_gnrc_udp
# icmpv6 error message support
USEMODULE += gnrc_icmpv6_error
# enable stats
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
# use IEEE802154 for sx126x instead of LoRa or RAW modes
USEMODULE += sx126x_ieee802154

# use a pseudomodule added for this application, which will allow packets to be intercepted
# when being sent from sixlowpan to the netif (impossible otherwise without adding an intermediate netif)
USEMODULE += gnrc_nettype_sixlowpan_prenetif

# use modules for displaying stats on board OLED displays

USEPKG += u8g2
USEMODULE += u8g2
USEMODULE += u8g2_utf8
USEMODULE += ztimer
USEMODULE += ztimer_msec
USEMODULE += ztimer_sec

# We use only the lower layers of the GNRC network stack, hence, we can
# reduce the size of the packet buffer a bit
# Set GNRC_PKTBUF_SIZE via CFLAGS if not being set via Kconfig.
ifndef CONFIG_GNRC_PKTBUF_SIZE
	CFLAGS += -DCONFIG_GNRC_PKTBUF_SIZE=512
endif

SRC += $(wildcard src/*.c)

# Set a default board ID, for configuration, or use whatever is supplied by an env variable
CONFIG_ID ?= 0
CFLAGS += -DCONFIG_ID=$(CONFIG_ID)

# Must set RIOTBASE before running to the directory RIOT is in
include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
