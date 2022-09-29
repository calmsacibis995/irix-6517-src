#!smake
#
# Makefile for libcrypt.
#
#ident	"$Revision: 1.3 $"

DEPTH=.

#  We cannot ship cryptbuf in libcrypt due to export regulations
# CFILES=crypt.c cryptio.c des_crypt.c des_encrypt.c cryptbuf.c enigma.c
CFILES=crypt.c cryptio.c des_crypt.c des_encrypt.c enigma.c

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

des_crypt.o: des_crypt.c
	if [ -s des_decrypt.c ] ;\
	then \
		$(CCF) -c des_crypt.c ;\
	else \
		$(CCF) -c -DINTERNATIONAL des_crypt.c ;\
	fi

