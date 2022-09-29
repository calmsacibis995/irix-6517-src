#
# Makefile for flashBuild
#
MHDEPTH=../..
include $(MHDEPTH)/mhcommondefs

OBJECTDIR=.

TARGETS= fb

CFILES=	\
	buildFlash.c\
	elfFiles.c\
	main.c\
	writeHex.c

#
# Override the global CFLAGS because tools don't follow the same rules
# as the firmware
#
CFLAGS= -g -I$(MHDEPTH)/include

default $(COMMONTARGS): $(COMMONPREF)$$@

mhdefault: $(TARGETS)

fb: 	$(OBJECTS)
	$(CC) $(OBJECTS) -L $(ROOT)/usr/lib -lelf -o $@

install: default

include $(MHCOMMONRULES)






