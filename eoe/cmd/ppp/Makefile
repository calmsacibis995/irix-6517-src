#!smake
# build PPP

# "$Revision: 1.23 $"

# the kernel interface is only 32-bits wide to avoid converting
# in mixed situations
OBJECT_STYLE=N32_M3

include	$(ROOT)/usr/include/make/commondefs

RSTRO	!=echo $(RELEASE) $(RELEASE_NAME)
RSTR	!=date '+"$RSTR0-%y%m%d"'

DEMONC	=ppp.c conf.c fsm.c lcp.c auth.c ipcp.c ccp.c parp.c pputil.c md5.c \
	callers.c conn.c getargs.c interface.c strsave.c sysfiles.c \
	ulockf.c uucpdefs.c uufake.c
DEMONO	=$(DEMONC:.c=.o)

STATC	=pppstat.c pppinfo.c
STATO	=$(STATC:.c=.o)

CFILES	=$(DEMONC) $(STATC)
LCDEFS	=-DPPP

KWORDS	=./keyword.h

LDIRT	=$(KWORDS)

TARGETS	=ppp pppstat

I_FLAGS	=-idb "slip.sw.ppp"

# The control file must not be readable by others because it may contain
# passwords.
CONF_FLAGS=-m 600 -idb "config(noupdate)"

LCOPTS	=-fullwarn -DRSTR='$(RSTR)'

default:$(TARGETS)

include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) $(I_FLAGS) -F /usr/etc $(TARGETS)
	$(INSTALL) $(CONF_FLAGS) -F /etc ppp.conf

ppp:	$(DEMONO)
	$(CCF) $(DEMONO) $(LDFLAGS) -lm -o $@

pppstat:$(STATO)
	$(CCF) $(STATO) $(LDFLAGS) -lcurses -o $@

lcp.o	:$(DEMONO:Nlcp.o)
conf.o ppp.o:$(KWORDS)

$(KWORDS):conf.c $(MAKEFILE) ppp.h
	rm -f $@
	echo '/* PPP configuration file keywords */' > $@
	echo					    >> $@
	echo 'enum keyword {'			    >> $@
	sed -n -e 's/[	]*case \(KEY[FW]_[_A-Z0-9]*\):.*/	\1,/p'	\
	    conf.c | sort -u			    >> $@
	echo '	END_KEY'			    >> $@
	echo '};'				    >> $@
	echo					    >> $@
	echo '#define KEYW 0'			    >> $@
	echo '#define KEYF 1'			    >> $@
	echo '#define KEYTBL_LEN (END_KEY+1)'	    >> $@
	date '+#define CONF_VERSION (1%y%j%H%M %% 1000000000)'	>> $@
	echo					    >> $@
	echo '#ifndef DEFINE'			    >> $@
	echo 'extern'				    >> $@
	echo '#endif'				    >> $@
	echo 'struct {'				    >> $@
	echo '	char *str;'			    >> $@
	echo '	enum keyword key;'		    >> $@
	echo '	int flag;'			    >> $@
	echo '} keytbl[KEYTBL_LEN] '		    >> $@
	echo '#ifdef DEFINE'			    >> $@
	echo ' = {'				    >> $@
	sed -e '/[	]*case KEY\([FW]\)_\([_A-Z0-9]*\):/!d'	\
	    -e 's/[	]*case //' -e 's/:.*//'			\
	    -e h						\
	    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	    -e 's/\(KEY.\)_.*/&, \1},/'				\
	    -e x						\
	    -e 's/KEY._\(.*\)/	{"\1", ##/' -e 's/"_/"-/'	\
	    -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/' \
	    -e G -e 's/##\n/	/' conf.c			\
	    | sort -u				    >> $@
	echo '	{"~", END_KEY, 0}'		    >> $@
	echo '}'				    >> $@
	echo '#endif'				    >> $@
	echo ';'				    >> $@
	echo '#undef KEYW'			    >> $@
	echo '#undef KEYF'			    >> $@
