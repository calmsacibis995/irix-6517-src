#!smake
include $(ROOT)/usr/include/make/commondefs

TARGETS = cdheadphone
CFILES =  cue.c cddata.c database.c olddb.c display.c main.c progdbox.c \
program.c seldbox.c skip.c util.c

LLDLIBS = -lcdaudio -lmediad -lXm -lPW -lXt -lX11 -lds
LCDEFS  = -D_BSD_TIME

CDPLAYER_IDB = -idb "std.sw.cdrom"

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 4755 -F /usr/bin/X11 $(CDPLAYER_IDB) cdheadphone
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults $(CDPLAYER_IDB) Cdplayer
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) add
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) addi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) ff
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) ffi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) icon
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) move_down
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) move_downi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) move_up
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) move_upi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) pause_status
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) play
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) play_status
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) playi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) remove
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) removei
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) rew
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) rewi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) skipb
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) skipbi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) skipf
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) skipfi
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) stop
	$(INSTALL) -m 444 -F /usr/lib/X11/app-defaults/cdplayer $(CDPLAYER_IDB) stopi

cdheadphone : $(OBJECTS)
	$(CCF) -o $@ $(OBJECTS) $(LDFLAGS)

# DO NOT DELETE THIS LINE -- make depend depends on it.
