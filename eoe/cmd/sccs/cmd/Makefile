#!smake
#ident	"$Revision: 1.25 $"
#
# Notes:
#

#
# Common Definitions
#
DEPTH=..
include	$(DEPTH)/slocaldefs

#
# Compile Time Options
#

#
# Local Definitions
#
LIBS=	../lib/comobj.a ../lib/cassi.a ../lib/mpwlib.a
TARGETS=	admin cdc comb delta get prs rmdel sact unget val vc what \
	rmchg sccsdiff help help2 sccs

LLDOPTS=-Wl,-woff,84 -Wl,-woff,85
LLDLIBS=$(LIBS)

#
# Targets/Rules
#

default: $(LIBS) $(TARGETS)

include $(COMMONRULES)

#
# Specific Target/Rules follow
#
install: default
	$(INSTALL) -dir "$(HELPLIB)/lib"
	$(INSTALL) -F /usr/bin admin
	$(INSTALL) -F /usr/bin cdc
	$(INSTALL) -F /usr/bin comb
	$(INSTALL) -F /usr/bin delta
	$(INSTALL) -F /usr/bin get
	$(INSTALL) -F /usr/bin prs
	$(INSTALL) -F /usr/bin rmdel
	$(INSTALL) -F /usr/bin sact
	$(INSTALL) -F /usr/bin unget
	$(INSTALL) -F /usr/bin val
	$(INSTALL) -F /usr/bin vc
	$(INSTALL) -F /usr/bin what
	$(INSTALL) -F /usr/bin sccsdiff
	$(INSTALL) -F /usr/bin help
	$(INSTALL) -F /usr/bin sccs
	$(INSTALL) -F $(HELPLIB)/lib help2

cdc:	rmchg
	-rm -f cdc
	ln rmchg cdc

rmdel:	rmchg
	-rm -f $@
	ln rmchg $@

sact:	unget
	-rm -f sact
	ln unget sact
