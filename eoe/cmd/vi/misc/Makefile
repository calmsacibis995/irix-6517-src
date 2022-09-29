#ident	"$Revision: 1.2 $"
#
# vi(1)/misc make script.
#
include	$(ROOT)/usr/include/make/commondefs

RM=	rm
CP=	cp
CFILES=	ctags.c fold.c mkstr.c xstr.c
CMDS=	ctags fold mkstr xstr cxref
XCMDS=	mkstr_host xstr_host

TARGETS = $(CMDS) $(XCMDS)

default: $(TARGETS)

install: default
	$(INSTALL) -F /usr/bin ctags
	$(INSTALL) -F /usr/bin fold
	$(INSTALL) -F /usr/bin mkstr
	$(INSTALL) -F /usr/bin xstr
#	$(INSTALL) -F /usr/bin cxref
	
# mkstr and xstr share the same intermediate .o file,
# which is bad!  Need to compile them one at a time
# to make it parallel-safe
mkstr_host: mkstr
xstr_host: xstr

${XCMDS}:
	if test -n "$$HOSTENV"; then \
		ROOTDIR=/usr /bin/cc ${@:_host=.c} -o $@; \
	else \
		${CCF} ${@:_host=.c} ${LDFLAGS} -o $@; \
	fi

include $(COMMONRULES)
