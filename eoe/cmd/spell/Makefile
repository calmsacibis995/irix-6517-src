#ident "$Revision: 1.8 $"

include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES=spell.c spellin.c spellout.c 
TARGETS=spell spellin spellout spellhost
HLIST=hlist hlista hlistb hstop
LDIRT=$(HLIST)

default:$(TARGETS) $(HLIST)

include $(COMMONRULES)

#
# This is because we will be building on a Cypress system
# while the target system will be a Sherwood.  Thus we
# will build spellin.c twice (one coff, one ELF).
spellhost: spellhost.o
	$(HOST_CC) spellhost.o -o $@
spellhost.o:
	$(HOST_CC) -c spellin.c -o $@

install: default
	$(INSTALL) -F /usr/lib spell

	$(INSTALL) -m 0444 -F /usr/share/lib/spell $(HLIST)
	$(INSTALL) -m 0444 -F /usr/share/lib/dict words
	${INSTALL} -F /usr/lib -lns ../share/lib/dict dict

	$(INSTALL) -F /usr/bin -src spell.sh spell
	$(INSTALL) -F /usr/bin spellin
	$(INSTALL) -F /usr/bin spellout
	
hlist:  words spellhost
	cat local* words | sort -u | ./spellhost >hlist

hlista: amer* local* hlist spellhost
	cat amer* local* | sort -u | ./spellhost hlist > hlista

hlistb: brit* local* hlist spellhost
	cat brit* local* | sort -u | ./spellhost hlist > hlistb

hstop: stop* spellhost
	cat stop* | sort -u | ./spellhost >hstop

