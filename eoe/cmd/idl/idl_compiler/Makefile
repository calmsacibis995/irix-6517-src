#!smake

#include "$(ROOT)/usr/include/make/commondefs"

CFILES = acf_y_tab.c lex_yy.c acf_lex_yy.c astp_com.c \
	astp_cpx.c astp_gbl.c astp_sim.c astp_dmp.c checker.c\
	command.c driver.c errors.c files.c frontend.c       \
	getflags.c keywds.c main.c message.c nametbl.c       \
	namdump.c propagat.c sysdep.c      \
	backend.c blocks.c bounds.c clihandl.c cspeldcl.c    \
	cspell.c cstubgen.c commstat.c decorate.c   \
	dflags.c dutils.c flatten.c genpipes.c hdgen.c       \
	ifspec.c inpipes.c localvar.c marshall.c mmarsh.c    \
	munmarsh.c mutils.c nodesupp.c nodmarsh.c nodunmar.c \
	oolrtns.c  outpipes.c sstubgen.c idlparse.c ksgen.c
#YFILES=acf.y nidl.y
#LFILES=acf.l nidl.l

LCINCS=-I. 
LCDEFS=-DSYS5 -DSGI -DSNI_SVR4_POSIX -DIDL_VERSION_TEXT='"ksgen 1.0"'
LDIRT=acf_y_tab.h acf_y_tab.c acf_lex_yy_i.c \
		lex_yy.c y_tab.h y_tab.c y.tab.c y.tab.h default_msg.h
_YFLAGS_=-d
SED=sed
MV=mv
TARGETS=ksidl

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)

$(TARGETS):$(OBJECTS)
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@


#
#dependencies and rules
#
${OBJECTS} : ./y_tab.h ./acf_y_tab.h

acf_lex_yy.o: ./acf_lex_yy_i.c ./acf_y_tab.h

acf_y_tab.o:./acf_y_tab.c

./acf_lex_yy_i.c:  acf.l
	${LEX} ${_LFLAGS_} -t ${.ALLSRC} > ${.TARGET}

./acf_y_tab.h:./acf_y_tab.c
	${MV} y.tab.h acf_y_tab.h
	touch acf_y_tab.h

./acf_y_tab.c:acf.y ./y_tab.h
	${YACC} ${_YFLAGS_} ${.ALLSRC}
	${MV} y.tab.c acf_y_tab.c

lex_yy.c: nidl.l
	${LEX} ${_LFLAGS_} -t ${.ALLSRC} > ${.TARGET}

./y_tab.h: ./y_tab.c
	${MV} y.tab.h y_tab.h
	touch y_tab.h

./y_tab.c:nidl.y
	${YACC} ${_YFLAGS_} ${.ALLSRC}
	${MV} y.tab.c y_tab.c

idlparse.o:	./y_tab.c ./y_tab.h

lex_yy.o: ./y_tab.h

# Add explicit dependencies for files depending on IDL_VERSION_TEXT,
# which is defined in sysdep.h.
command.o driver.o:	sysdep.h

# This rule creates an include file containing the text of messages in
# the message file, so that IDL can print default messages.
# The following sed command (note that $ must be doubled in the Makefile):
#	1. Deletes lines beginning with $.
#	2. Deletes blank lines.
#	3. Changes lines of the form:
#		<number> <message test>
#	    to
#		"message text",		
./default_msg.h default_msg.h: nidlmsg.m
	${SED}  -e '/^\$$/d;/^$$/d;s/^[^ ]* /"/;s/$$/",/;' \
		${.ALLSRC} > ${.TARGET}

message.o:	default_msg.h

# Generate the message catalog
idl.cat: nidlmsg.m
	${GENCAT} ${.TARGET} ${.ALLSRC}
