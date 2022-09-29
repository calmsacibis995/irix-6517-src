#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/initpkg/rc3.d/RCS/Makefile,v 1.12 1992/11/11 17:48:29 curtis Exp $"

# make script for rc3.d stuff

#
# Sequence number utilization
#
# These are all the known users of sequence numbers in the /etc/rc0.d
# directory.  Please keep this list up to date...
#
#	Name		Description
#
# * - unbundled products that must fit into this sequence
#

include	$(ROOT)/usr/include/make/commondefs

# XXX if you define these you must uncomment the for loops under install:
#STARTLST =
#STOPLST = 

INSDIR = /etc/rc3.d
FRMDIR = /etc/init.d


default:$(FRC)

clean:

clobber:clean

FRC:

install: default
	$(INSTALL) -idb "std.sw.unix" -dir $(INSDIR)
#	for f in $(STOPLST); do \
#		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
#		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
#	done
#	for f in $(STARTLST); do \
#		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
#		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
#	done
