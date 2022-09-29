# build rhost

# "$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

MKDEPFILE = Makedepend

CFILES	= main.c conf.c

KWORDS	= keyword.h

LDIRT	= $(KWORDS)
LCDEFS	= -DDEBUG -fullwarn

TARGETS	=rhost

# The control file must not be readable by others because it may contain
# passwords.
I_FLAGS	=-idb "trix.sw.rhost"
CONF_FLAGS=-m 600 -idb "config(noupdate)"

all: $(TARGETS)

default: $(TARGETS)

include $(COMMONRULES)

install: all
	$(INSTALL) $(I_FLAGS) -F /usr/etc rhost
	$(INSTALL) $(CONF_FLAGS) -F /etc rhost.conf

rhost:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

sdump:	sdump.o
	$(CCF) sdump.o $(LDFLAGS) -lelf -o $@

conf.o main.o: $(KWORDS)

$(KWORDS): conf.c $(MAKEFILE) rhost.h
	rm -f $@
	echo '/* RHOST configuration file keywords */' > $@
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
	    -e G -e 's/##./	/' conf.c			\
	    | sort -u				    >> $@
	echo '	{"-", END_KEY, 0}'		    >> $@
	echo '}'				    >> $@
	echo '#endif'				    >> $@
	echo ';'				    >> $@
	echo '#undef KEYW'			    >> $@
	echo '#undef KEYF'			    >> $@
