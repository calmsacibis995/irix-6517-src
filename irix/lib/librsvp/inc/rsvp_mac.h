/*
 * @(#) $Id: rsvp_mac.h,v 1.6 1998/11/25 08:43:36 eddiem Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version: Steven Berson & Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#ifndef __rsvp_mac_h__
#define __rsvp_mac_h__

/* bitmap routines */

#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))

/* timer comparison -- 32bit modular arithmetic
 */
#define LT(X,Y)       ((int)((X)-(Y)) < 0)
#define LTE(X,Y)      ((int)((X)-(Y)) <= 0)

/*
 * Macros for fast min/max.
 */
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/*
 * FD_ZERO uses bzero, but we wish to avoid its use completely for consistancy
 */
#define bzero(sp, len)     memset((void *)(sp), 0, (size_t)(len))

/* misc macros */

#define Get_local_addr (IF_toip(0))

/* Local IP address corresponding to given vif # or if #
 */
#define VIF_toip(vif)  IF_toip(vif_toif[vif])
#define IF_toip(if)  if_vec[if].if_orig.s_addr

/*
 *	Operations on objects
 */
#define Obj_Data(x)	(((char *)(x))+sizeof(Object_header))   /*SH*/
#define After_Obj(x,cls)	((char *)(x)+Object_Size(Ver_Obj(x,cls)))
#define Ver_Obj(x,cls)	((Obj_Class(x)== class_##cls) ? (x) : NULL)
#define Object_of(x)	(Object_header *)(x)
#define Next_Object(x)  Object_of((char *) (x) + Obj_Length(x))
#define Object_Size(x)  ((x)? Obj_Length(x): 0)

#define copy_spec(x) (FlowSpec *) copy_object((Object_header *) x)
#define copy_filter(x) (FilterSpec *) copy_object((Object_header *) x)
#define copy_tspec(x) (SENDER_TSPEC *) copy_object((Object_header *) x)
#define copy_adspec(x) (ADSPEC *) copy_object((Object_header *) x)
#define copy_scope(x) (SCOPE *) copy_object((Object_header *) x)

#define Move_Object(f,t) move_object((Object_header *)(f),(Object_header *)(t))


/* Initialize an object at location x, with given class and ctype.
 *	Zeros rest of object. (Assumes length given by class, i.e,
 *	uses max length).
 */
#define Init_Object(x, cls, ctype)  \
		memset((x), 0, sizeof(cls)); \
		((Object_header *) x)->obj_class = class_##cls;\
		((Object_header *) x)->obj_ctype = ctype;\
		((Object_header *) x)->obj_length = sizeof(cls);

#define Init_Var_Obj(x, cls, ctype, len)  \
		memset((x), 0, len); \
		((Object_header *) x)->obj_class = class_##cls;\
		((Object_header *) x)->obj_ctype = ctype;\
		((Object_header *) x)->obj_length = len;

/*
 *   Set_Sockaddr(struct sockaddr_in *, u_long *, int):
 *		Macro to set up sockaddr_in structure
 *		(Bury system-dependence here)
 */
#ifdef SOCKADDR_LEN
#define Set_Sockaddr_in(sadp, host, port) \
		{ memset((char *)(sadp), 0, sizeof(struct sockaddr_in));\
		(sadp)->sin_family = AF_INET; (sadp)->sin_addr.s_addr = host;\
		(sadp)->sin_port = port;\
		(sadp)->sin_len = sizeof(struct sockaddr_in); }
#else
#define Set_Sockaddr_in(sadp, host, port) \
		{ memset((char *)(sadp), 0, sizeof(struct sockaddr_in));\
		(sadp)->sin_family = AF_INET; (sadp)->sin_addr.s_addr = host;\
		(sadp)->sin_port = port; }
#endif

/*
 *	Log memory full condition (maybe should just increment counter?)
 */
#define Log_Mem_Full(where) log(LOG_WARNING, 0, "Memory full @ %s\n", where);
	
/*
 * Control debug printout
 */
#define IsDebug(type)  (l_debug >= LOG_DEBUG && (debug & (type)))

/*
 *	Pack and unpack rsvp_errno = 256*Error_val + Error_Code
 */
#define Set_Errno(code, val) ((val)<<8 | (code))
#define Get_Errcode(errno) ((errno)&255)
#define Get_Errval(errno) ((errno)>>8)

/*
 *	Session hash function
 *		Add half words and do simple congruential hash.
 *		This will be more complex for IPv6...
 */
#define Sess_hashf(sesp) ((((sesp)->sess4_addr.s_addr >> 16) + \
		((sesp)->sess4_addr.s_addr & 0xffff) + \
		(sesp)->sess4_prot)%SESS_HASH_SIZE)

#endif	/* __rsvp_mac_h__ */
