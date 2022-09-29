#
#ident	"$Revision: 1.6 $ $Author: olson $"
#
# Makefile for terminfo.
#
include	$(ROOT)/usr/include/make/commondefs

#
# Compile Time Options
#
COMPILE=	$(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/tic -v -s

#
# Local Definitions
#
TERMDIR=	/usr/share/lib/terminfo
TABDIR=		/usr/share/lib/tabset
PARTS=		header *.ti trailer

#
# Targets/Rules
#

default: terminfo.src
	rm -rf dest; mkdir dest;
	TERMINFO=`pwd`/dest $(COMPILE) terminfo.src > errs 2>&1
	$(ECHO) Expect 2 warnings on iris-ansi-24-net and iris-ansi-66-net
	sh ./ckout

clean:
	rm -f terminfo.src errs

clobber: clean
	rm -rf dest

FRC:

#
# Specific Target/Rules follow
#

#
# NB. tic installs its output files in $TERMINFO
#
# Find all terminals and determine whether or not they should be made
# part of standard software or the terminfo subsystem.  A check is also
# made for symbolic links which are preserved in the install.  If the base
# entry is in the standard list, all of the links go with it.
install: default
	cd dest;							\
	    find . ! -type d -print | sed 's@\./@@' |			\
	    while read term; do						\
		if [ -l $$term ];					\
		then							\
			set -- `ls -l $$term`; shift 10;		\
			realterm=$$1;					\
		else							\
			realterm=$$term;				\
		fi;							\
		if [ -l $$term ];					\
		then							\
			$(INSTALL) -idb nostrip -F $(TERMDIR) -lns $$realterm $$term; \
		else							\
			$(INSTALL) -m 444 -idb nostrip -F $(TERMDIR) $$term;	\
		fi;							\
	    done
	cd tabset; \
	    find . ! -type d -print | sed 's@\./@@' | \
	    while read files; do \
		$(INSTALL) -m 444 -idb nostrip -F $(TABDIR) "$$files"; \
	    done


terminfo.src:	$(PARTS)
	cat $(PARTS) > terminfo.src
