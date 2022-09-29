#
# Makefile for Moosehead debugcard.
DEBUGCARD	=	..
include			$(DEBUGCARD)/mhdbcommondefs

#
# some target macros.
TARGETDIR=$(DEBUGCARD)/$(PRODUCT).O
TARGETS=$(TARGETDIR)/diags
OBJECTDIR=$(TARGETDIR)
VPATH=$(OBJECTDIR)

# NOTE: -O2 does not works, please do not change -O1 to -O2.
OPTIMIZES  = -O1
ARCSINC    = $(WORKAREA)/stand/arcs/include
IP32INC    = $(WORKAREA)/stand/arcs/IP32/include
SYSINC     = $(ROOT)/usr/include
DIAGINCS   = -I. -I$(IP32INC) -I$(ARCSINC) -I$(SYSINC)
PROSRTYPE  = -DR4000 -DR4400 -DR4000PC -DR4600 -DR4600SC -DTRITON
LIBRARYS   =  -D_STANDALONE -DSTANDALONE -D_K32U64 -D_KERNEL -D__STDC__=1

DIAGSDEFS  = -DDEBUGCARD
DBLLCCFLAGS= $(OPTIMIZES) $(DIAGSDEFS) $(PROSRTYPE) $(LIBRARYS)

#
# Assembly sources.
#
#	mem_Multwr.s	\
#	mem_Multwr1.s	\
#	mem_Multwr2.s	\
#	mem_Multwr3.s	\
#	mem_alias1.s	\
#

ASFILES=			\
	diagasm.s		\
	mem_BEtst.s		\
	mem_ECCtst6.s	\
	mem_ECCtst7.s	\
	mem_ECCtst8.s	\
	mem_addr1.s		\
	mem_addr2.s		\
	mem_addr3.s		\
	mem_addr4.s		\
	mem_addr5.s

#
# C sources.
#	pcidiags.c
#   mem_alias2.c    \
#   mem_alias21.c   \
#   mem_alias22.c   \
#   mem_Eblktst.c   \
#

CFILES=				\
    diagcntl.c		\
    diagmenu.c		\
    pcidiags.c		\
    mem_ECCtst10.c	\
    mem_ECCtst111.c	\
    mem_ECCtst12.c  \
    mem_ECCtst13.c  \
    mem_ECCtst14.c  \
    mem_ECCtst21.c  \
    mem_ECCtst22.c  \
    mem_ECCtst3.c   \
    mem_ECCtst4.c   \
    mem_ECCtst5.c   \
    mem_ECCtst9.c   \
    mem_REGtst.c    \
    memorytst.c


#
# Here is the place we pickup any local flags for CFLAGS and ASFLAGS.
# SIMULATION will get defined by the Makefile in triton.sim directory.
DBLCCFLAGS=$(SIMULATION) $(DBLLCCFLAGS)
DBLASFLAGS=$(SIMULATION) $(DBLLCCFLAGS)

#
# Some default setting common for all mh stuff.
default: $(TARGETS)
clean:   mhclean
	cd $(OBJECTDIR) ;\
	rm -f $(OBJECTS)
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	rm -rf $(TARGETS)


#
# Now heres something special just for the debugcard monitor.
$(TARGETDIR)/diags:		$(OBJECTS)
		${CD} $(OBJECTDIR);\
		${LD} $(LDRFLAGS) -o diags $(OBJECTS)

#
# Include more common stuff from MH make tree.
include $(DEBUGCARD)/mhdbcommonrules
