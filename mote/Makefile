CONTIKI_PROJECT = sensor-mote root-mote coordination-mote
PROJECT_SOURCEFILES = routing.c hashmap.c trickle-timer.c
all: $(CONTIKI_PROJECT)

#CONTIKI_WITH_RIME = 1
WERROR=0
MAKE_MAC ?= MAKE_MAC_CSMA
#MAKE_MAC = MAKE_MAC_TSCH
MAKE_NET = MAKE_NET_NULLNET
CONTIKI = ../../../
include $(CONTIKI)/Makefile.include
