#!smake
#ident	"$Revision: 1.30 $"
#
#

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/mtune
.PATH:$(KPATH)

MTUNE= \
	autofs \
	bsd \
	cachefs \
	disp \
	cell \
	efs \
	if_ec2 \
	if_ptr \
	msg \
	nfs \
	nfs3 \
	numa \
	lockd \
	pipefs \
	psema \
	sem \
	shareII \
	snfs \
	svckudp \
	uds  \
	usync  \
	kernel \
	xfs
#
# Rules
#
KCOMMONPREF=m
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	cd $(KPATH) ;\
	RAWIDB= $(INSTALL) -F $(MTUNEDIR) -m 444 -idb "config(update)" $(MTUNE) ; \
	RAWIDB= $(INSTALL) -F $(SYSGENDIR) -m 644 -idb "config(noupdate)" stune ;

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	cd $(KPATH) ;\
	$(INSTALL) -F $(MTUNEDIR) -m 444 -idb "config(update)" $(MTUNE) ; \
	$(INSTALL) -F $(SYSGENDIR) -m 644 -idb "config(noupdate)" stune ; \
	$(INSTALL) -F $(SHIP_MTUNEDIR) -m 444 -idb "config(update)" $(MTUNE) ; \
	$(INSTALL) -F $(SHIP_SYSGENDIR) -m 644 -idb "config(noupdate)" stune ; \

mrmtargets!
	if test -d $(ROOT)/$(MTUNEDIR); then \
		cd $(ROOT)/$(MTUNEDIR); \
		rm -f $(MTUNE); \
	fi

mclean!
mclobber:kclobber
mlint!mlint
