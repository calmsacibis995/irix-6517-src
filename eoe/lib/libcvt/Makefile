# This makefile doesn't do much, because all it does is install the
# old goldengate dso's.  They have to go into $ROOT, as well as real
# images.

default headers clean clobber:

exports rawidb install:
	$(INSTALL) -m 444 -F /usr/lib/internal -idb "nostrip" -src libcvto32.so libcvt.so
	$(INSTALL) -m 444 -F /usr/lib32/internal -idb "nostrip" -src libcvtn32.so libcvt.so
