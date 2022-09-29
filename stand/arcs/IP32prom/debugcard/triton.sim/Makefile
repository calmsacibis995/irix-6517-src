#
# Makefile for Moosehead debugcard.
DEBUGCARD	= ..
FLASHIMG	= $(DEBUGCARD)/$(PRODUCT).O/IP32flash.o
SUBDIRS		= lcore diags IP32fw
include		$(DEBUGCARD)/mhdbcommondefs

#
# some target macros.
TARGETS		=	SIM
TARGETDIR	=	$(DEBUGCARD)/triton.sim/$(PRODUCT).O
OBJECTDIR	=	$(TARGETDIR)
VPATH		=	$(OBJECTDIR)

DBLCCFLAGS=-xansi -fullwarn -D_STANDALONE -DSTANDALONE -DMP -D_K32U64 -D_KERNEL -DR4000 -DR4000PC -DR4600 -DR4600SC -DTRITON -DIP32 -D__STDC__=1 -DDEBUGCARD

#
#

#
# Sources for lcore directory.
LASFILES=\
	bev_trap_vector.s   \
	cache_init.s		\
    asm_support.s

LCFILES=\
    UARTsim.c           \
    uartio.c            \
    utilities.c         \
    flash.c             \
	memory.c			\
    menu.c

#
# Sources for diags directory.
DASFILES=	\
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

DCFILES=	\
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
# Sources for building SIM
ASFILES=	$(LASFILES) \
			$(DASFILES)

CFILES=		$(LCFILES)  \
			$(DCFILES) 

#
# Define Sim flags
SIMULATION=	-DSim

#
# Some default setting common for all mh stuff.
default: $(TARGETS)
clean:   mhclean
	cd $(OBJECTDIR) ;\
	rm -f $(OBJECTS);\
	rm -f $(TARGETS) lcore diags
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	rm -rf $(TARGETS)

#
# Now heres something special just for the debugcard monitor.

SIM:		$(OBJECTDIR) lcore diags $(FLASHIMG)
	cd   	$(OBJECTDIR) ;\
	${LD}	$(LDOBJFLAGS) $(OBJECTS) ../$(FLASHIMG) -o $@
	mv		$(OBJECTDIR)/$@ .

lcore:
	${CD}	$(DEBUGCARD)/$@ ;\
	${MAKE}	"ASFILES=$(LASFILES)" "CFILES=$(LCFILES)" "TARGETDIR=$(TARGETDIR)" "SIMULATION=$(SIMULATION)"
	${RM}	$(TARGETDIR)/$@

diags:
	${CD}	$(DEBUGCARD)/$@ ;\
	${MAKE}	"ASFILES=$(DASFILES)" "CFILES=$(DCFILES)" "TARGETDIR=$(TARGETDIR)" "SIMULATION=$(SIMULATION)"
	${RM}	$(TARGETDIR)/$@

IP32fw:
	${CD}	$(DEBUGCARD)/$@ ;\
	${MAKE}

debug:
	echo "$(FLASHIMG) \n $(OBJECTS) \n $(MKDEPFILE) \n $(CCFLAGS) \n kuang"

#
# Have to defined my own rules.

include $(DEBUGCARD)/mhdbcommonrules
