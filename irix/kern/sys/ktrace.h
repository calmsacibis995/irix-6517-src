/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_SYS_KTRACE_H_
#define	_SYS_KTRACE_H_
#ident "$Id: ktrace.h,v 1.5 1996/12/07 00:40:35 sp Exp $"


/*
 * Trace buffer entry structure.
 */
typedef struct ktrace_entry {
	void	*val[16];
} ktrace_entry_t;

/*
 * Trace buffer header structure.
 */
typedef struct ktrace {
	lock_t		kt_lock;	/* mutex to guard counters */
	int		kt_nentries;	/* number of entries in trace buf */
	int		kt_index;	/* current index in entries */
	int		kt_rollover;
	ktrace_entry_t	*kt_entries;	/* buffer of entries */
} ktrace_t;

/*
 * Trace buffer snapshot structure.
 */
typedef struct ktrace_snap {
	int		ks_start;	/* kt_index at time of snap */
	int		ks_index;	/* current index */
} ktrace_snap_t;
	
/*
 * Exported interfaces.
 */
extern ktrace_t *ktrace_alloc(int, int);

#ifndef DEBUG

#define	ktrace_free(ktp)
#define	ktrace_enter(ktp,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15)

#define KTRACE2(buf,a,b)
#define KTRACE4(buf,a,b,c,d)
#define KTRACE6(buf,a,b,c,d,e,f)
#define KTRACE8(buf,a,b,c,d,e,f,g,h)
#define KTRACE10(buf,a,b,c,d,e,f,g,h,i,j)
#define KTRACE12(buf,a,b,c,d,e,f,g,h,i,j,k,l)
#define KTRACE14(buf,a,b,c,d,e,f,g,h,i,j,k,l,m,n)
#define KTRACE16(buf,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)

#else /* DEBUG */

extern void ktrace_free(ktrace_t *);

extern void ktrace_enter(
	ktrace_t	*,
	void		*,
	void		*,
        void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*,
	void		*);	     

extern void ktrace_print_buffer(struct ktrace *, __psint_t, int, int);

/* these are all located in idbg.c */
extern ktrace_entry_t   *ktrace_first(ktrace_t *, ktrace_snap_t *);
extern int              ktrace_nentries(ktrace_t *);
extern ktrace_entry_t   *ktrace_next(ktrace_t *, ktrace_snap_t *);
extern ktrace_entry_t   *ktrace_skip(ktrace_t *, int, ktrace_snap_t *);

#define KTRACE2(buf,a,b) \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#define KTRACE4(buf,a,b,c,d) \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#define KTRACE6(buf,a,b,c,d,e,f) \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#define KTRACE8(buf,a,b,c,d,e,f,g,h) { \
	void	*H; \
	H = (void *)(__psint_t)(h); \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		(g), H, \
		0, 0, 0, 0, 0, 0, 0, 0); \
}

#define KTRACE10(buf,a,b,c,d,e,f,g,h,i,j) { \
	void	*H, *J; \
	H = (void *)(__psint_t)(h); \
	J = (void *)(__psint_t)(j); \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		(g), H, \
		(i), J, \
		0, 0, 0, 0, 0, 0); \
}

#define KTRACE12(buf,a,b,c,d,e,f,g,h,i,j,k,l) { \
	void	*H, *J, *L; \
	H = (void *)(__psint_t)(h); \
	J = (void *)(__psint_t)(j); \
	L = (void *)(__psint_t)(l); \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		(g), H, \
		(i), J, \
		(k), L, \
		0, 0, 0, 0); \
}

#define KTRACE14(buf,a,b,c,d,e,f,g,h,i,j,k,l,m,n) { \
	void	*H, *J, *L, *N; \
	H = (void *)(__psint_t)(h); \
	J = (void *)(__psint_t)(j); \
	L = (void *)(__psint_t)(l); \
	N = (void *)(__psint_t)(n); \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		(g), H, \
		(i), J, \
		(k), L, \
		(m), N, \
		0, 0); \
}

#define KTRACE16(buf,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) { \
	void	*H, *J, *L, *N, *P; \
	H = (void *)(__psint_t)(h); \
	J = (void *)(__psint_t)(j); \
	L = (void *)(__psint_t)(l); \
	N = (void *)(__psint_t)(n); \
	P = (void *)(__psint_t)(p); \
	ktrace_enter((buf), \
		(a), (void *)(__psint_t)(b), \
		(c), (void *)(__psint_t)(d), \
		(e), (void *)(__psint_t)(f), \
		(g), H, \
		(i), J, \
		(k), L, \
		(m), N, \
		(o), P); \
}


#endif /* DEBUG */
#endif
