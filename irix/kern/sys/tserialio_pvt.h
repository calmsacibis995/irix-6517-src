/*
   tserialio_pvt.h - internal defines for tserialio
*/

#include <sys/time.h> /* for stamp_t */

/* tserialio driver ---------------------------------------------------- */

#include <sys/ktserialio.h>

#define TSIO_FLAGS_RS422       0x00000001 /* RS422 and not RS232 */
#define TSIO_FLAGS_INTR_DEBUG  0x00000002 /* debugging--1 second stat */
#define TSIO_FLAGS_TX_DEBUG    0x00000004 /* debugging--print tx chars */

#define TSIO_USED_CFLAGS (CBAUD|CSIZE|CSTOPB|PARENB|PARODD)

typedef struct tsio_comm_params_s
{
  /* 
   * cflag -- same as libtserialio library cflag except CBAUD is always
   *          ignored (use ospeed instead)
   */
  tcflag_t cflag;     

  /* integer baud figure such as 9600 or 38400.  0==hangup is not supported */
  speed_t ospeed;

  /*
   * other flags - one of TSIO_FLAGS_* above
   *
   * includes RS422, which you want for MIDI on IP22 or Sony deck control.
   */
  int flags;

  /*
   * extclock_factor -- control serial port clocking
   *                    same as libtserialio library extclock_factor
   */
  u_int extclock_factor;

} tsio_comm_params_t;

/*
   ioctl: acquire user rb.

   a user-mode program should first open the device, then
   do this ioctl() on the file descriptor to set up the port, then
   do map() to map the rb, then go to it.
*/

#define TSIO_ACQUIRE_RB 1       /* ioctl: open user rb */

typedef struct tsio_acquireurb_s
{
  int nlocs; /* number of bytes in data rb and number of stamp_t's in stamp rb.
                Does not include the header.  It's only the number of
                samples in the ring buffers.  It does include the 
                one guard location though (this takes 1 byte in the data
                buffer and 1 stamp_t in the timestamp buffer). */

  char direction; /* TSIO_TOMIPS or TSIO_FROMMIPS */
  tsio_comm_params_t commparams; /* baud, stop, parity, ... */
} tsio_acquireurb_t;

#define TSIO_FILLUNITS_BYTES 1 
#define TSIO_FILLUNITS_UST   2


/*
  user ring buffer header: present in user-mode program's address space
  right before rb data and stamps themselves.
*/
typedef struct urbheader_s
{
  int head;                     /* index of buffer head location           */
  int tail;                     /* index of buffer tail location           */
  stamp_t fillpt;               /* buffer fill point:                      */
                                /*   TOMIPS:   wakeup if nfilled >= fillpt */
                                /*   FROMMIPS: wakeup if nfilled <  fillpt */
                                /* !! ***NOTE*** TOMIPS differs from AL !! */
                                /* !!  this is required to support UST  !! */
  int fillunits;                /* TSIO_FILLUNITS_*                        */
  int intreq;                   /* set while a poll is pending             */
} urbheader_t;

/* libtserialio library ------------------------------------------------ */

#define TS_LIB_DEBUG  0x00000001
#define TS_INTR_DEBUG 0x00000002
#define TS_TX_DEBUG   0x00000004

