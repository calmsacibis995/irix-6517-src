#! smake
# Makefile for netvisualyzer FTRs (icons, workspace rules)
#   $Revision: 1.15 $
#
#
#	from:	     Makefile Template for Icons ( from IconSmith )
# 			Copyright (c) 1990, 1991 SGI
#
# (copied from /usr/lib/filetype/templates/Makefile)
#
#
# Notice: This is from a beta-test version of IconSmith.

DEPTH= ..
include ${DEPTH}/Makedefs

FILETYPE = /usr/lib/filetype
WORKSPC = /usr/lib/workspace


# ----------------------------------------------------

# [] Insert your Raw Type Rule files below in first-match-wins order.
RTR_FILES = analyzer.rtr netgraph.rtr netlook.rtr \
			netsnoop.rtr rtquery.rtr traceroute.rtr ttcp.rtr \
			netcollect.rtr netpack.rtr netaccount.rtr \
			netcpaData.rtr netgraphHist.rtr netsnoopData.rtr \
			nettop.rtr nvlicense.rtr tutorial.rtr browser.rtr \
			netlookData.rtr netfilters.rtr netfiltersData.rtr \
			netvis.rtr

TARGETS = netvisualyzer.ftr
TEMPLATE_FILE = netVisualyzer.wsrc

EXITOP = exitop("(cd $$rbase/usr/lib/filetype; make; :) >/dev/null 2>&1")

TARGET_DIR = $(FILETYPE)/install


#BIN = /usr/local/bin
BIN = .
SHELL = /bin/sh


FTR = ${ROOT}/usr/sbin/ftr

.SUFFIXES: .rtr .ftr

FTRFILES = $(RTR_FILES:.rtr=.ftr)
CTRTARGET = $(TARGETS:.ftr=.ctr)
LDIRT = $(FTRFILES)


.ORDER: .rtrdepend $(TARGETS)

default: .rtrdepend $(TARGETS)

include $(COMMONRULES)

depend incdepend: .rtrdepend

.rtrdepend: $(RTR_FILES)
	@for i in $(RTR_FILES); do 					\
		mkdepend -c $(BIN)/ftr.depend -i $(MKDEPFILE) $$i;	\
	done

.rtr.ftr:
	${BIN}/ftr.merge $<

$(TARGETS): $(FTRFILES)
	cat $(FTRFILES) > $@

ctr: $(TARGETS)
	$(FTR) $(TARGETS) -o $(CTRTARGET)
	
include ${DEPTH}/Makedefs

install: $(TARGETS) $(TEMPLATE_FILE)
	# Install in target directory so miniroot ftr make works
	${INSTALL} -idb '${DISPLAYIDB} ${EXITOP}' -m 444 \
				-F ${TARGET_DIR} ${TARGETS}
	${INSTALL} -idb ${DISPLAYIDB} -m 444 \
				-F ${TARGET_DIR} ${TEMPLATE_FILE}
