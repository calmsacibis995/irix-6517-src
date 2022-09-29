#!smake

BASEVERSION=n32bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=cmsd

WOFF=
#OPTIMIZER=
LCOPTS+=-fullwarn -DDEBUG

# Kernel files that need to be symlinked to this directory.
KFILES=cms_daemon.c cms_mesg.c cms_membership.c cms_comb_dynamic.c cms_trace.c

HFILES= cms_info.h cms_trace.h cms_base.h cms_message.h cms_membership.h

LOBJECTS=cms_main.o cms_daemon.o cms_mesg.o cms_ep.o cell_set.o cms_kernel.o \
	cms_membership.o cms_comb_dynamic.o cms_trace.o cms_test.c

default: $(TARGETS)

include $(CMDCOMMONRULES)

all: cmsd


cmsd: $(LOBJECTS)
	${CCF} $(LOBJECTS) -o $@

$(KFILES): $(HFILES) cell_set.h cell_set.c
	ln -f -s ../../kern/cell/cms/$@ .

$(HFILES):
	ln -f -s ../../kern/cell/cms/$@ .

cell_set.h :
	ln -f -s ../../kern/ksys/cell/cell_set.h .

cell_set.c :
	ln -f -s ../../kern/cell/cell_set.c .
