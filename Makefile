# name of your application
APPLICATION = robot


# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../RIOT

# map
NUM_LINES ?= 21
NUM_COLUMNS ?= 21
SURVIVOR_LIST ?= 2,2,11,11,20,5
MINES_LIST ?= 1,17,17,18
MAX_MINES ?= 4
ENERGY ?= 120


ROBOT_PORT ?= 10000
ROBOT_ID ?= 0

CFLAGS += -DNUM_LINES=$(NUM_LINES)
CFLAGS += -DNUM_COLUMNS=$(NUM_COLUMNS)
CFLAGS += -DSURVIVOR_LIST=\"$(SURVIVOR_LIST)\"
CFLAGS += -DMINES_LIST=\"$(MINES_LIST)\"
CFLAGS += -DROBOT_PORT=$(ROBOT_PORT)
CFLAGS += -DROBOT_ID=$(ROBOT_ID)
CFLAGS += -DMAX_MINES=$(MAX_MINES)
CFLAGS += -DENERGY=$(ENERGY)

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
# Activate ICMPv6 error messages
USEMODULE += gnrc_icmpv6_error
# Specify the mandatory networking modules for IPv6 and UDP
USEMODULE += gnrc_ipv6_router_default
USEMODULE += gnrc_udp
USEMODULE += gnrc_sock_udp

# Add a routing protocol
USEMODULE += gnrc_rpl
USEMODULE += auto_init_gnrc_rpl
# This application dumps received packets to STDIO using the pktdump module
USEMODULE += gnrc_pktdump
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += netstats_rpl

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Uncomment the following 2 lines to specify static link lokal IPv6 address
# this might be useful for testing, in cases where you cannot or do not want to
# run a shell with ifconfig to get the real link lokal address.
#IPV6_STATIC_LLADDR ?= '"fe80::cafe:cafe:cafe:1"'
#CFLAGS += -DGNRC_IPV6_STATIC_LLADDR=$(IPV6_STATIC_LLADDR)

# Uncomment this to join RPL DODAGs even if DIOs do not contain
# DODAG Configuration Options (see the doc for more info)
# CFLAGS += -DCONFIG_GNRC_RPL_DODAG_CONF_OPTIONAL_ON_JOIN

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
