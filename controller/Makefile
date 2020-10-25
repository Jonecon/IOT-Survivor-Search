# controller Makefile to test a small map (21x21)
# the number of survivors and mines concerns only the implementation of
# scenario by the robot, so it's ommitted here
#
# name of your application
APPLICATION = controller

#controller port
CONTROLLER_PORT ?= 10000

# map information that is available to the controller
# e.g. may be used in its logic to slice the map among robots
NUM_LINES ?= 21
NUM_COLUMNS ?= 21
SPEED ?= 2

# maximum number of robots that can be created
MAX_ROBOTS ?= 15

# list of IPv6 addresses of robots "addr1,addr2,addr3..."
# the actual addresses need to be placed here once the robots
# are started
ROBOT_ADDRESSES=fe80::cc93:6aff:feac:bca2,fe80::58f5:b5ff:fea1:98ae,fe80::94ce:f7ff:fea8:e7a9,fe80::acf1:d5ff:fe46:d199,fe80::c858:f0ff:fe04:df16

# list of ports that are used by robots. the port number do not
# need to change every time. a robot will use the n-th number in the
# list (like the ip addresses), where robot 0 is n==0
ROBOT_PORTS=10000,10001


CFLAGS += -DNUM_LINES=$(NUM_LINES)
CFLAGS += -DNUM_COLUMNS=$(NUM_COLUMNS)
CFLAGS += -DMAX_ROBOTS=$(MAX_ROBOTS)
CFLAGS += -DROBOT_ADDRESSES=\"$(ROBOT_ADDRESSES)\"
CFLAGS += -DROBOT_PORTS=\"$(ROBOT_PORTS)\"
CFLAGS += -DCONTROLLER_PORT=$(CONTROLLER_PORT)
CFLAGS += -DSPEED=$(SPEED)

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../RIOT

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

# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# USEMODULE += ps
# USEMODULE += netstats_l2
# USEMODULE += netstats_ipv6
# USEMODULE += netstats_rpl

# Add a routing protocol
# USEMODULE += gnrc_rpl
# USEMODULE += auto_init_gnrc_rpl
# This application dumps received packets to STDIO using the pktdump module
# USEMODULE += gnrc_pktdump
# Additional networking modules that can be dropped if not needed
# USEMODULE += gnrc_icmpv6_echo

# Uncomment this to join RPL DODAGs even if DIOs do not contain
# DODAG Configuration Options (see the doc for more info)
# CFLAGS += -DCONFIG_GNRC_RPL_DODAG_CONF_OPTIONAL_ON_JOIN

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
