# This makefile is used only to export header files needed to
# build the stand arcs fx from within irix/cmd/fx.
# none of these install rules ever generate anything to go to
# customers, just for internal build ROOTs.  Dave Olson

#ident "$Revision: 1.3 $"

DEPTH=../
COMMONPREF=sinc
include ../commondefs
STANDINC = /usr/include/standarcs

exports install:
	(cd arcs; $(INSTALL) -idb STANDFRIEND -F $(STANDINC)/arcs -m 444 errno.h folder.h fs.h)
	(cd sys; $(INSTALL) -idb STANDFRIEND -F $(STANDINC)/sys -m 444 buf.h dksc.h scsi.h)
	$(INSTALL) -idb STANDFRIEND -F $(STANDINC) -m 444 libsc.h saio.h saioctl.h setjmp.h

# nop everything else
.DEFAULT:
