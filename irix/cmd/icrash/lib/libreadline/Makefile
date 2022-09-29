#!smake
## -*- text -*- ####################################################
#								   #
# Makefile for libreadline and history libraries.			   #
#								   #
####################################################################
include $(ROOT)/usr/include/make/commondefs

# Here is a rule for making .o files from .c files that doesn't force
# the type of the machine (like -sun3) into the flags.
# .c.o:
#	$(CC) -c $(CFLAGS) $(LOCAL_INCLUDES) $(CPPFLAGS) $*.c

# Destination installation directory.  The libraries are copied to DESTDIR
# when you do a `make install', and the header files to INCDIR/libreadline/*.h.
DESTDIR = /usr/local/lib
INCDIR = /usr/local/include

# Define TYPES as -DVOID_SIGHANDLER if your operating system uses
# a return type of "void" for signal handlers.
TYPES = -DVOID_SIGHANDLER

# Define USG as -DUSG if you are using a System V operating system.
USG = -DUSG

# HP-UX compilation requires the BSD library.
#LOCAL_LIBS = -lBSD

# Xenix compilation requires -ldir -lx
#LOCAL_LIBS = -ldir -lx

# Comment out "-DVI_MODE" if you don't think that anyone will ever desire
# the vi line editing mode and features.
READLINE_DEFINES = $(TYPES) -DVI_MODE

# DEBUG_FLAGS =
# LDFLAGS = $(DEBUG_FLAGS) 
# CFLAGS = $(DEBUG_FLAGS) $(USG) -I.
LCDEFS = $(USG) -I. -I.. -g -n32 -cckr -Dsparc -Dsun -Dsgi

# A good alternative is gcc -traditional.
# CC = gcc -traditional
# CC = cc
RANLIB = /usr/bin/ranlib
AR = ar
RM = rm
CP = cp

CFILES = readline.c history.c funmap.c keymaps.c
OFILES = $(CFILES:.c=.o)
TARGETS = ../../libreadline.a

##########################################################################

COMMONPREF=libreadline_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)

install: $(DESTDIR)/libreadline.a includes
