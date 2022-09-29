#
# Makefile for Moosehead debugcard tools.
DEBUGCARD=	..
MHDEPTH=	$(DEBUGCARD)/..
DTOOLS=		$(DEBUGCARD)/tools
IP32FW=		$(MHDEPTH)/images

#
# Defined some local unix commands which are not part of 
# MH mhcommondefs/mhcommonrules.
CD		=	cd
CC		=	cc
IMG2ASM	=	$(DTOOLS)/img2asm
TOUCH	=	touch

#
# Some target macros.
TARGETDIR=.
OBJECTDIR=$(TARGETDIR)
VPATH=$(OBJECTDIR)

#
# Sources

SRCS	=	img2asm.c

#
# Objects and Make default targets.

OBJS	=	$(SRCS:.c=.o)
TARGETS	=	$(SRCS:.c=)


#
# Some default setting common for all mh stuff.
default: $(TARGETS)
clean:
	rm -f $(OBJS)
rmtargets: $(_FORCE)
	rm -rf $(TARGETS)
clobber: clean rmtargets

#
# Local make targets.

img2asm:	img2asm.c
	${CC}	-o $@ img2asm.c

checkitout:	checkitout.c
	${CC}	-o $@ checkitout.c

