DESTROOT=..
DESTPATH=

all: crashme

clean:
	-rm -f *.o crashme core *~ *.BAK *.CKP

install: crashme crashme.1
	install -c -s crashme   $(DESTROOT)/$(DESTPATH)/bin/crashme
#	install -c    crashme.1 $(DESTROOT)/$(DESTPATH)/man/man1/crashme.1

crashme:	crashme.o
	cc -o crashme crashme.o
