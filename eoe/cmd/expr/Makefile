#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/expr/RCS/Makefile,v 1.18 1998/10/09 04:21:07 leedom Exp $"

include	$(ROOT)/usr/include/make/commondefs

YFILES=expr.y
TARGETS=$(YFILES:.y=)

default:	$(TARGETS)

.y:
	$(YACCF) $@.y
	$(CCF) y.tab.c $(LDFLAGS) -o $@

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin expr
	$(INSTALL) -F /usr/bin -lns ../../sbin/expr expr
