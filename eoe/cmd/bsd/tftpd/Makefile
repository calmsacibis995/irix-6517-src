#!smake
#
# Make the tftp demon
#
# $Revision: 1.10 $

include	$(ROOT)/usr/include/make/commondefs

CFILES	=tftpd.c tftpsubs.c
TARGETS	=tftpd

LCOPTS	=-signed

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/etc -lns ../../var/boot boot
	$(INSTALL) -F /var/boot -m 755 -src tftpd.inst.sh -idb \
		'postop($$rbase/var/boot/.tftpd.inst.sh)' .tftpd.inst.sh
	$(INSTALL) -F /usr/etc $(TARGETS)

include $(ROOT)/usr/include/make/commonrules

tftpd:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
