/*
 * Copyright 1997-1998 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
/*
 * tserialio - timestamped, non-STREAMS, low-overhead serial port
 *
 * QUICK REFERENCE GUIDE:
 *
 * to set up serial port params, you make a TSconfig, set it up, and pass
 * the config into tsOpenPort() to actually open a TSport.
 *
 * cut and paste this code:
 */
#if 0
{
  TSconfig config = tsNewConfig();
  TSport port;

  tsSetPortName(config, "/dev/ttyts1");          /* required */
  tsSetDirection(config, TS_DIRECTION_TRANSMIT); /* required */
  tsSetQueueSize(config, 200);                   /* required */
  tsSetCflag(config, CS8|PARENB|PARODD);         /* required */
  tsSetOspeed(config, 38400);                    /* required */
  tsSetProtocol(config, TS_PROTOCOL_RS232);      /* optional */
  tsSetExternalClocking(config, 0);              /* optional */

  if (TS_SUCCESS != tsOpenPort(config, &port))
    exit(2);

  tsFreeConfig(config);

  /* now use tsWrite() if you opened a TS_DIRECTION_TRANSMIT port
   * or tsRead() if you opened a TS_DIRECTION_RECEIVE port.
   */
  ...

  tsClosePort(port);
}
#endif

#include <sys/termios.h> /* for tcflag_t */
#include <sys/time.h> /* for stamp_t */
#include <sys/ktserialio.h> /* some kernel defines */

typedef struct _TSconfig *TSconfig;
typedef struct _TSport *TSport;

typedef int TSstatus;

/*
 * Error Handling:
 *
 * Functions that can err return a TSstatus.  TS_SUCCESS means success.
 * On failure, functions return one of the tokens below, and also
 * set oserror() to the value of that token.
 *
 * Errors are also reported as they occur by the execution of a 
 * process-global, non-thread-safe error handler callback which 
 * you can set.  This is only really useful for debugging, where
 * it is indispensible.  It is sometimes also useful for command-line 
 * programs.
 *
 * The default error handler prints an error to stderr.
 * When defining an error handler, you can specify whether or not
 * to include the TS function name that is erring in the string.
 * Most applications will want to turn off the error handler 
 * in non-debug compiles using something like this:
 *
   #ifdef NDEBUG
      mvrSetErrorHandler(NULL, FALSE);
   #endif
 *
 * Programmatic errors, where you pass an out-of-range, nonsensical,
 * or otherwise illegal value to an MVR library call, all return
 * TS_ERROR_BAD_LIBRARY_CALL.  To get the juicy details for these 
 * errors, use the error handler and read the resulting string.  
 * The idea is that these errors should never occur in finished 
 * developer code, or if they do, the developer is not interested 
 * in detailed case-by-case programmatic handling.
 *
 */
typedef void (*TSerrfunc)(TSstatus status, const char *message);
/* includefuncname tells whether to include the TS funcname in message */
TSerrfunc tsSetErrorHandler(TSerrfunc newfunc, int includefuncname); 
/* gets current error handler and includefuncname setting */
TSerrfunc tsGetErrorHandler(int *includefuncname /*can be NULL*/);

#define TS_SUCCESS 0
#define TS_ERROR_BAD_LIBRARY_CALL 1
#define TS_ERROR_PORT_BUSY 2
#define TS_ERROR_OUT_OF_MEM 3
#define TS_ERROR_OPENING_PORT 4
#define TS_ERROR_SELECT_FAILED 5

/*
 * make a new TSconfig.  you must set certain parameters before using
 * this config with tsOpenPort.  see tsOpenPort.
 * see above for code example.
 *
 * can fail with TS_ERROR_OUT_OF_MEM
 */
TSconfig tsNewConfig(void); /* 0 on failure */
/*
 * make a new TSconfig which has all the parameters of a currently
 * open TSport.
 *
 * can fail with TS_ERROR_OUT_OF_MEM
 */
TSconfig tsNewConfigFromPort(TSport port); /* 0 on failure */
/*
 * copy one TSconfig to another.  You'll have to free the returned
 * TSconfig separately.
 *
 * can fail with TS_ERROR_OUT_OF_MEM
 */
TSconfig tsCopyConfig(TSconfig from); /* 0 on failure */
/*
 * free a TSconfig.
 */
TSstatus tsFreeConfig(TSconfig config); /* != TS_SUCCESS on failure */

/* set UNIX filename of timestamped serial port to open:
 *
 *   - this should be one of the nodes /dev/ttyts*
 *   - /dev/ttytsN refers to the same physical port as /dev/tty[fd]N
 *   - opening the STREAMS device for a given serial port
 *     (/dev/tty[fd]N) and opening a TSport to that port are
 *     mutually exclusive.  tsOpenPort will return
 *     TS_ERROR_PORT_BUSY if the port is already in use as a
 *     STREAMS device.
 *   - assuming the STREAMS device for a given port is not
 *     open, then you can tsOpenPort() that port as many times
 *     as you want for input, as long as the communications parameters
 *     match those used in the first open.  tserialio will allow only one
 *     open for writing at a time.  Again, unsuccessful
 *     attempts in all these cases will return TS_ERROR_PORT_BUSY
 *
 * can fail with TS_ERROR_OUT_OF_MEM
 */
TSstatus tsSetPortName(TSconfig config, char *name);

/* 
 * specify tx/rx direction of timestamped serial port
 *
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if token bad
 */
#define TS_DIRECTION_TRANSMIT TSIO_FROMMIPS /* output */
#define TS_DIRECTION_RECEIVE  TSIO_TOMIPS   /* input */
TSstatus tsSetDirection(TSconfig config, int direction);

/* specify queue size of timestamped serial port
 *
 * when you create a TSport, tserialio allocates a queue.  this
 * parameter specifies the number of (byte,timestamp) pairs which
 * this queue can hold.
 *
 * for an input port, this queue holds the bytes which have been
 * received on the serial port but which you have not yet read.  
 * if you allow this queue to fill up to capacity, and then 
 * further characters arrive on the serial port, those new
 * characters will be discarded.  therefore, it is very important
 * to choose a queue capacity and a frequency of reading the
 * queue which will keep the queue from overflowing.
 *
 * for an output port, this queue holds the bytes which you have 
 * written but which have not yet been transmitted out the serial port.
 * if you attempt to write so much data that this queue would fill
 * past its capacity, then tsWrite() will block until enough
 * space has become available to write all of your data.
 *
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if qsize <0
 */
TSstatus tsSetQueueSize(TSconfig config, int queuesize);

/*
 * most communication parameters are specified here using
 *   traditional struct termios.c_cflag flags: 
 *     CSIZE bits (CS5, CS6, CS7, CS8)
 *     CSTOPB bit (1==2 stop bits, 0==1 stop bits)
 *     PARENB (0==no parity, 1==see PARODD)
 *     PARODD (1==odd parity, 0==even parity)
 *     CBAUD (B9600 etc.) is IGNORED
 *           this field of c_cflag has been obsoleted. use tsSetOspeed instead.
 */
TSstatus tsSetCflag(TSconfig config, tcflag_t cflag);

/*
 * baud rate, specified as a straight integer in symbols per
 * second (examples: 9600, 31250 (MIDI), 38400 (video deck control)).
 * this parameter replaces the CBAUD bits of TS_CFLAG.  this parameter
 * exists because the termio flags omit several necessary baud rates.
 * baud rate of 0, meaning "hang up" or "disconnect," is not supported.
 *
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if ospeed==0
 */
TSstatus tsSetOspeed(TSconfig config, speed_t ospeed);

/*
 * electrical protocol to use on the port:
 *   TS_PROTOCOL_RS232 (the default)
 *   TS_PROTOCOL_RS422
 * for MIDI on Indigo, Indy, and Indigo2, choose TS_PROTOCOL_RS422
 * for video deck control, choose TS_PROTOCOL_RS422
 *
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if token bad
 */
#define TS_PROTOCOL_RS232 0
#define TS_PROTOCOL_RS422 1
TSstatus tsSetProtocol(TSconfig config, int protocol);

/*
 * normally, an asynchronous serial port runs off its own internal
 * clock.  in some situations, such as MIDI on Indigo, Indy, and
 * Indigo2, the port clocks off of an external signal present on
 * one of the pins of the serial port.  this parameter specifies
 * where the serial port should look for its clock source.
 *
 * 0 (the default) means the serial port should use its internal clock
 * N (N > 1) means the serial port should clock itself off
 * of the provided external clock divided by N.
 *
 * if this parameter is nonzero, then the CBAUD bits of the TS_CFLAG
 * parameter and the TS_SPEED parameter are ignored.
 *
 * for Indigo, Indy, and Indigo2, N can be 1, 16, 32, or 64.  
 *
 * Macintosh-compatible MIDI dongles produce a 1 MHz external
 * clock signal.  therefore, when you plug such dongles into an 
 * Indigo, Indy, or Indigo2, you want to choose a clock factor of 32 so
 * that you get the MIDI baud rate of (1,000,000/32==) 31.25kHz.
 *
 * for video deck control, choose 0.
 *
 * for O2 MIDI, choose 0, since you need to use an RS232->Mac dongle.
 *
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if extclock_factor<0
 */
TSstatus tsSetExternalClockFactor(TSconfig config, int extclock_factor);

/*
 * tsOpenPort: open a timestamped serial port
 *
 * each TSport represents a connection to one physical serial port
 * in one direction.  here are the parameters:
 *
 * --- flow control for RS-232 operation
 *
 * tserialio ignores the state of DCD and CTS; it assumes both signals
 * are always asserted, so data can always be received or transmitted.
 *
 * tserialio does not provide control over the DTR or RTS lines.
 * when a port is open under tserialio, these lines are always asserted
 * on the physical serial port.
 *
 * control may be added at a later time.  for now, the main applications
 * of tserialio (MIDI and deck control) are 422 applications, and do not 
 * require flow control.
 *
 * can fail with TS_ERROR_OUT_OF_MEM
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if args invalid
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if portname not specified in config
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if direction not specified in config
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if queuesize not specified in config
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if cflag not specified in config
 * can fail with TS_ERROR_BAD_LIBRARY_CALL if ospeed not specified in config
 * can fail with TS_ERROR_OPENING_PORT if communication with driver fails
 * can fail with TS_ERROR_OPENING_PORT if you use feature not supported on hw
 */      
TSstatus tsOpenPort(TSconfig config, TSport *returnPort);
/* 
 * close TSport.
 */
TSstatus tsClosePort(TSport port);

/* 
 * returns same quantity as TS_QUEUE_SIZE parameter to tsOpenPort--this
 * quantity is fixed for a given port and will not change over time.
 */
int tsGetQueueSize(TSport port);

/* 
 * returns the total number of (byte,timestamp) pairs currently in
 * the TSport's queue.
 */
int tsGetFilledBytes(TSport port);

/*
 * tsRead() reads nbytes (byte,timestamp) pairs from the specified port.
 * the function returns the data of each byte in data[i], and the UST
 * time at which the byte came in the input jack of the machine in
 * stamps[i].  the timestamp on each byte is accurate to plus or minus 
 * one millisecond.  if nbytes (byte,timestamp) pairs are not currently
 * available in the port's queue, then tsRead() will block until
 * it has been able to read all nbytes pairs.
 *
 * tsWrite writes nbytes (byte,timestamp) pairs to the specified port.
 * tserialio will then output data[i] at the UST time given by stamps[i],
 * with an accuracy of plus or minus one millisecond.  if sufficient
 * space is not available in the port's queue to write all nbytes
 * (byte,timestamp) pairs immediately, then tsWrite() will block
 * until it has been able to write all nbytes pairs.  the timestamps
 * you specify should be non-decreasing; the behavior is not defined
 * if the timestamps ever decrease.
 *
 * these UST timestamps can be used to measure or schedule serial i/o
 * relative to other signals on the system.  see dmGetUST(3dm), 
 * alGetFrameTime(3dm), and vlGetUSTMSCPair(3dm).
 *
 * see below for an important NOTE about exactly what accuracy and
 * latency guarantees tserialio offers.
 *
 * if tsRead() or tsWrite() need to block, they will call select().
 * if that select fails for any reason other than EINTR, the calls
 * will return with TS_ERROR_SELECT_FAILED.
 */
TSstatus tsRead(TSport port, unsigned char *data, stamp_t *stamps, 
                int nbytes);
TSstatus tsWrite(TSport port, unsigned char *data, stamp_t *stamps, 
                 int nbytes);

/*
 * tsGetFD() returns a file descriptor which you can pass to select()
 * or poll() if you want to block until data becomes available in
 * an input port, or space becomes available in an output port.
 *
 * before calling select() or poll(), you must first call tsSetFillPointBytes()
 * to specify when you want to unblock:
 *
 * INPUT PORTS:  will unblock from select() or poll() when
 *               tsGetFilledBytes() >= tsGetFillPoint()
 *
 * OUTPUT PORTS: will unblock from select() or poll() when
 *               tsGetFilledBytes() <  tsGetFillPoint()
 * 
 * the calls tsWrite() and tsRead() may change the fillpoint, so you should
 * make sure to call tsSetFillPointBytes() before each invocation of select()
 * or poll().
 *
 * *NOTE* the definition of output fillpoint differs from that in the AL.
 * the AL file descriptor unblocks when there are more than "fillpoint"
 * spaces in the queue.  this change was necessary to facilitate
 * a future feature of this library: the ability to choose fillpoints 
 * in units of time rather than data.
 *
 * tsSetFillPointBytes() can fail with TS_ERROR_BAD_LIBRARY_CALL if point bad
 */
TSstatus tsSetFillPointBytes(TSport port, int nbytes);
int tsGetFillPointBytes(TSport port);
int tsGetFD(TSport port);

/*
 * === NOTE: accuracy and latency
 *
 * there is a crucial distinction between accuracy and latency
 * which is relevant here.  the function of tserialio is to deliver
 * the accuracy guarantees described above, which are not normally
 * achievable for an IRIX process using the normal serial port
 * interfaces.
 *
 * tserialio was not designed to offer guarantees about the latencies 
 * your application sees.  it has no interactions whatsoever with the
 * UNIX scheduler.  it is simply a service which pairs together bytes
 * of data and UST times in such a way that your application can
 * manipulate the pair atomically.  therefore,
 *
 * - for input ports, tserialio offers no guarantees about the maximum 
 *   time between when a byte arrives at the port and when tsRead unblocks
 *
 * - for input and output ports, tserialio offers no guarantees about the
 *   minimum time between when a port reaches its fillpoint and when
 *   a TSport's file descriptor unblocks a select() or poll().
 * 
 * - when writing a given (byte, timestamp) pair to an output port
 *   using tsWrite(), you must provide tserialio with enough "advance
 *   warning" (ie, the difference between the current UST at the time
 *   of tsWrite() and the UST timestamp in the pair must be large enough)
 *   so that tserialio can schedule output of the data on time.  tserialio
 *   offers no guarantee that any particular amount of "advance warning" 
 *   will always be enough.
 * 
 * having laid out these theoretical limitations, we now point out
 * some practical realities.  the facts described below apply to the 
 * current implementation of tserialio and are not guaranteed to be
 * true for all future implementations, although it is quite likely 
 * that they will.
 * 
 * first, tserialio is built on a mechanism which is extremely lightweight 
 * compared to that behind the standard serial interface (/dev/tty[nd]*).
 * the mechanism is similar to the lightweight mapped ringbuffers offered 
 * by the Audio Serial Option (see asoserns(7)).  it is extremely
 * likely that the latencies described above will be shorter in
 * the average case when using tserialio.
 *
 * second, we have found that for certain common applications, such
 * as playing a MIDI file or controlling a Sony RS-422 VTR, the
 * actual latency figures required to perform the task are practically
 * available to a properly configured IRIX process (see schedctl(2)).
 *  
 * finally, tserialio has some properties which the standard serial
 * interface lacks which may increase its usefulness in applications
 * for other reasons.  tsGetFilledBytes() performs no system calls and 
 * is extremely efficient.  a tsRead() which can be satisfied by data 
 * currently in the port's queue is little more than a bcopy and 
 * requires no system calls.  a tsWrite() for which there is room 
 * in the port's queue is similarly efficient.  therefore, 
 * an application which periodically polls tsGetFilledBytes() 
 * can perform all of its serial i/o without any system calls.  this
 * may be desireable for applications which spend most of their time
 * doing heavy CPU or i/o activity anyway, for which a convenient periodic
 * opportunity for polling the serial device is available without spinning
 * on the CPU.  For example, this may be the case with a video deck control
 * application.
 *
 * real latency guarantees may be available in a future IRIX release.
 * for now, tserialo provides the critical functionality for many
 * timely serial applications.
 * 
 */
 

