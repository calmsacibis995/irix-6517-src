#!smake
#
# AIM Suite III v3.1 Makefile
# (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
# All Rights Reserved.
# Created on Mon Mar 28 14:23:15 PST 1994
#

# 64bit
#CC=$(TOOLROOT)/usr/bin/cc -64
#LDOPT=-nostdinc -I$(ROOT)/usr/include -nostdlib -L$(ROOT)/usr/lib64

# 32bit
LDOPT=-nostdinc -I$(ROOT)/usr/include -nostdlib -L$(ROOT)/usr/lib
CC=$(TOOLROOT)/usr/bin/cc -32

SYS=-DSYS5
TIMETYPE=-DT_TIMES
CCOPT=-O1


DHZ=-DHZ=100
CFLAGS= $(CCOPT) $(TIMETYPE) $(LDOPT) $(DHZ) $(SYS)

TASKS = add.o disk1.o div.o fun.o funcal.o mul.o \
	pipe_cpy.o ram.o creat-clo.o disk_src.o

OBJS = multiuser.o vmctl.o tp_ctl.o ttyctl.o ttyinit.o ttyconf.o rand.o rtmsec.o $(TASKS)

TARGS = blend filter gettime lptest multiuser plot tp_drvr ttydrvr vmdrvr

# ugly but portable, even on (very) dumb makes..

all: tsks multiuser .firsttime
	./blend

.firsttime: report lptest blend gettime
	chmod a+x REPT
	@touch .firsttime
#	tar -xvf fakeh.tar

mix:
	./blend

tsks:   $(TASKS) suite3.h

report:	plot filter

.c.o:	suite3.h
	$(CC) $(CFLAGS) -c $*.c

multiuser: suite3.h vmdrvr tp_drvr ttydrvr $(OBJS)
	$(CC) $(CFLAGS) -o multiuser $(OBJS)

blend: blend.c
	$(CC) $(CFLAGS) -o blend blend.c

calc_hz: calc_hz.c
	$(CC) $(CFLAGS) -o calc_hz calc_hz.c

plot: plot.c 
	$(CC) $(CFLAGS)  -o plot plot.c
 
filter: filter.c
	$(CC) $(CFLAGS) filter.c -o filter

vmctl.o vmdrvr.o: vmtest.h vmem.h testerr.h

vmdrvr: vmdrvr.o vmem.o
	$(CC) $(CFLAGS) vmdrvr.o vmem.o -o vmdrvr

tp_drvr: tp_drvr.c tp_test.h testerr.h
	$(CC) $(CFLAGS) tp_drvr.c -o tp_drvr

lptest: lptest.c
	$(CC) $(CFLAGS) lptest.c -o lptest

tp_ctl.o: tp_test.h testerr.h

clean:
	touch xyz.o; \
	rm -f *.o

clobber: clean
	rm -f $(TARGS)

install: all
	/etc/install -idb "diag.sw.diag mach(MODE=64bit)" \
	-F /usr/diags/regression/aim3 $(TARGS); \
	cp config $(ROOT)/usr/diags/regression/aim3; \
	cp mixb $(ROOT)/usr/diags/regression/aim3; \
	cp workfile $(ROOT)/usr/diags/regression/aim3; \
	cp fakeh.tar $(ROOT)/usr/diags/regression/aim3; \
	(cd $(ROOT)/usr/diags/regression/aim3; tar -xvf fakeh.tar; \
	rm fakeh.tar)

OBJS1 = ttyctl.o ttydrvr.o ttyinit.o ttyconf.o

$(OBJS1): ttytest.h testerr.h

ttydrvr: ttydrvr.o ttyinit.o ttyconf.o
	$(CC) $(CFLAGS) ttydrvr.o ttyinit.o ttyconf.o -o ttydrvr

newrun:
	@echo "Resetting AIM Suite III for another run."
	@rm -f tmp* core *.o

reset:
	@echo "Resetting AIM Suite III to new install condition."
	@rm -f .firsttime
	@rm -f workfile suite3.ss toutput results
	@rm -f plot filter blend calc_hz input multiuser nohup.out
	@rm -f RUN SAVE runit
	@rm -f ttydrvr vmdrvr tp_drvr lptest
	@rm -f tmp* core *.o
	@rm -f input.auto output.auto nfs.stat
	@rm -rf fakeh
