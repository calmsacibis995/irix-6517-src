CFLAGS = -g -I. -DEMULATE -n32

ptest: ptest.o promlog.o fprom.o
	cc -n32 -o ptest ptest.o promlog.o fprom.o

promlog.o: promlog.c promlog.h fprom.h
ptest.o: ptest.c promlog.h fprom.h
fprom.o: fprom.c fprom.h

promlog.c:
	ln -sf ../promlog.c .
promlog.h:
	ln -sf ../promlog.h .
fprom.c:
	ln -sf ../../lib/libkl/ml/fprom.c .
fprom.h:
	ln -sf ../../include/fprom.h .

clean:
	-rm -f promlog.c promlog.h fprom.c fprom.h
	-rm -f *.o ptest core a.out
	-rm -f prom
