#ident	"$Revision: 1.27 $"

include	$(ROOT)/usr/include/make/commondefs

LCOPTS=-fullwarn
INSTOPT	=-g mail -u root 
DIRS	=/var/mail /var/mail/:saved

CFILES	=mail.c rmail.c
FILES	=mail rmail
TARGETS	=$(FILES)

default:$(FILES)

include $(COMMONRULES)

install: default
	$(INSTALL) $(INSTOPT) -m 775 -dir $(DIRS)
	$(INSTALL) $(INSTOPT) -m 775 -idb "delhist" -dir /usr/mail/:saved
	$(INSTALL) $(INSTOPT) -m 6755 -F /usr/bin mail
	$(INSTALL) $(INSTOPT) -m 755 -F /usr/bin rmail
