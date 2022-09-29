/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1993 Spider Systems Limited
 *
 *  LLC2MATCH_IF.C
 *
 *    Ethernet and LLC1 matching code interface include file
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/0/s.llc2match_if.h
 *	@(#)llc2match_if.h	1.1
 *
 *	Last delta created	15:47:57 5/6/93
 *	This file extracted	16:55:35 5/28/93
 *
 */

#if defined(__STDC__) || defined(__cplusplus)
# define _P(s) s
#else
# define _P(s) ()
#endif

#ifdef NO_PROTO_PROMOTE
#define UINT8	uint8
#define UINT16	uint16
#else
#define UINT8	int
#define UINT16	int
#endif

/* llc2_match.c */
caddr_t init_match _P((void));
void end_match _P((caddr_t handle));
queue_t *lookup_sap _P((caddr_t handle, uint16 *lengthp, uint8 *start1,
	UINT16 length1, uint8 *start2, UINT16 length2));
int register_sap _P((caddr_t handle, queue_t *q, uint8 *start, UINT16 length));
int deregister_sap _P((caddr_t handle, queue_t *q, uint8 *start,
	UINT16 length));
void deregister_q _P((caddr_t handle, queue_t *q));
#ifdef SGI
void unregister_q _P((caddr_t handle, queue_t *uq, queue_t *dq, UINT16 length));
#endif
#ifdef DEBUG
void print_match_all _P((caddr_t handle));
#endif
