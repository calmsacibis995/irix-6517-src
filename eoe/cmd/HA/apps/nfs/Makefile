#ident	"$Revision: 1.3 $"

include	$(ROOT)/usr/include/make/commondefs


# simple files not changed by the user
STDSIMP	=nfs
MONITOR	=ha_nfs_lmon ha_nfs_rmon

INSDIR	=/var/ha/resources
INSCMD	=$(INSTALL) -m 755 -F $(INSDIR)


default:$(FILES)

clean clobber:

FRC:

install: default
	$(INSTALL) -dir $(INSDIR)
	$(INSCMD) $(STDSIMP)
	$(INSTALL) -m 755 -F /var/ha/actions $(MONITOR)

debug:
	exec $(MAKE) LCDEFS='$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
