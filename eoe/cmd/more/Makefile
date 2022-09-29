
include $(ROOT)/usr/include/make/commondefs

TARGETS		= more
TARGET_DATA	= more.help
CFILES		= ch.c command.c decode.c help.c input.c line.c linenum.c \
		  main.c option.c os.c output.c position.c prim.c screen.c \
		  signal.c tags.c ttyin.c

LCDEFS		= -D_BSD_COMPAT
LCINCS		= -I.
LLDLIBS		= -ltermcap -lgen

default : $(TARGETS)

include $(COMMONRULES)

$(TARGETS) : $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
	${INSTALL} -F /usr/lib $(TARGET_DATA)
	$(INSTALL) -F /usr/bin -ln $(TARGETS) page

