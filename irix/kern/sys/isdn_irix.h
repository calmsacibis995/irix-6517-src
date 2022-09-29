/* Copyright (c) Silicon Graphics, Inc., 1994, all rights reserved. */
/*
   isdn_irix.h

     Things from the ISDN root ism which have to be available even when
     ISDN root isn't.  (Really just a lame way to get PPP some info it
     needs without annoying the source tree police.)  
*/

#ifndef ISDN_IRIX_H

/* An message passed upstream once per second in an M_CTL when maximum rx
 * thruput is reached on a channel.  For the benefit of if_ppp.o.
 * Currently, this is the only M_CTL sent upstream.  All other errors
 * indications and control messages are passed in M_PROTOs and
 * M_PCPROTOs -wsm7/28/94.  
 */
#define ISDN_ACTIVE_RX  0x1

struct isdn_mctl
{
  char type;			/* message type */
};

#endif /* ISDN_IRIX_H */
