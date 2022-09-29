       EXEC = go
       OBJS = lat_op.o lat_op_asm.o cycle.o

         CC = cc
        ABI = -n32
    LDFLAGS = $(ABI)
     CFLAGS = $(ABI) -O -g $(DEFINES)

    DEFINES = 

   LIBS = # -lfetchop

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

sn0:
	$(MAKE) DEFINES="$(DEFINES) -DSN0" LIBS="$(LIBS) -lfetchop"

.c.o:
	$(CC) $(CFLAGS) -c $<

.s.o:
	$(CC) $(CFLAGS) -c $<

clean:
	/bin/rm -f $(OBJS) go
