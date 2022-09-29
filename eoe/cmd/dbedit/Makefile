#!smake
#ident	"$Revision: 1.6 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES =	main.c process_db.c read_db.c save_db.c \
		change_summary.c audit_changes.c confirm.c database.c
TARGETS = dbedit
LLDLIBS =

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin -m 4755 -u root -g sys dbedit
	$(INSTALL) -F /etc -m 644 -u root -g sys -src dbedit.conf dbedit

dbedit:	${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@
