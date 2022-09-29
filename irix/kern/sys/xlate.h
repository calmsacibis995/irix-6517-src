/*
 * xlate.h
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.13 $"

#ifndef _SYS_XLATE_H
#define _SYS_XLATE_H

#ifdef _KERNEL

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xlate_info_s {
	int	inbufsize;	/* Size of buffer passed to client */
	int	copysize;	/* size to copy in/out: filled in by client */
	void	*smallbuf;	/* Buffer passed to client */
	void	*copybuf;	/* Passed null to client, client sets to
				 * point to copy buffer */
	int	abi;		/* target abi - passed on from argument to
				 * copyin_xlate or xlate_copyout */
} xlate_info_t;

/* 
 * The called function returns a boolean value - non-zero for
 * failure, else zero.
 * xlate_copyout also returns a boolean value - non-zero for failure.
 */

enum xlate_mode { SETUP_BUFFER, DO_XLATE };

typedef int (*xlate_out_func_t)(void *, int, xlate_info_t *);
typedef int (*xlate_in_func_t)(enum xlate_mode, void *, int, xlate_info_t *);

extern int	xlate_copyout(void *, void *, int,
			      xlate_out_func_t, int, int, int);
extern int	copyin_xlate(void *, void *, int,
			      xlate_in_func_t, int, int, int);

#if (_MIPS_SIM == _ABI64)
#define XLATE_COPYOUT(from,to,size,func,abi,count)	       \
			xlate_copyout(from,to,size,func,abi,ABI_IRIX5_64,count)

#define COPYIN_XLATE(from,to,size,func,abi,count)	       \
			copyin_xlate(from,to,size,func,abi,ABI_IRIX5_64,count)

#define XLATE_FROM_IRIX5(_xlate_func, _user_struct, _native_struct)    \
	_user_struct = _xlate_func(_user_struct, _native_struct)

#define XLATE_TO_IRIX5(_xlate_func, _user_struct, _native_struct)      \
	(void)_xlate_func(_native_struct, _user_struct)
#else

#define XLATE_COPYOUT(from,to,size,f,a,c)	\
		(copyout((from), (to), (size)) ? EFAULT : 0)
#define COPYIN_XLATE(from,to,size,f,a,c)	\
		(copyin((from), (to), (size)) ? EFAULT : 0)
#define XLATE_FROM_IRIX5(_xlate_func, _uap, _native_uap)
#define XLATE_TO_IRIX5(_xlate_func, _user_struct, _native_struct)

#endif  /* _ABI64 */


#define COPYIN_XLATE_PROLOGUE(SOURCE_STRUCT, TARGET_STRUCT)		\
	struct SOURCE_STRUCT *source;					\
	struct TARGET_STRUCT *target;					\
									\
	ASSERT(info->smallbuf != NULL);					\
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);		\
									\
	if (mode == SETUP_BUFFER)					\
	{ ASSERT(info->copybuf == NULL);				\
	  ASSERT(info->copysize == 0);					\
	  if (sizeof(struct SOURCE_STRUCT) <= info->inbufsize)		\
	      info->copybuf = info->smallbuf;				\
	  else								\
	      info->copybuf = kern_malloc(sizeof(struct SOURCE_STRUCT)); \
	  info->copysize = sizeof(struct SOURCE_STRUCT);		\
	  return 0;							\
	}								\
									\
	ASSERT(info->copysize == sizeof(struct SOURCE_STRUCT));		\
	ASSERT(info->copybuf != NULL);					\
									\
	target = to;							\
	source = info->copybuf;


#define COPYIN_XLATE_VARYING_PROLOGUE(SOURCE_STRUCT, TARGET_STRUCT, SRC_SIZE) \
	struct SOURCE_STRUCT *source;					      \
	struct TARGET_STRUCT *target;					      \
	size_t                        size;				      \
                                                                              \
	ASSERT(info->smallbuf != NULL);                                       \
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);                     \
									      \
	size = SRC_SIZE;						      \
	if (mode == SETUP_BUFFER)                                             \
	{ ASSERT(info->copybuf == NULL);                                      \
	  ASSERT(info->copysize == 0);                                        \
	  if (size <= info->inbufsize)                                        \
	      info->copybuf = info->smallbuf;                                 \
	  else                                                                \
	      info->copybuf = kern_malloc(size);                              \
	  info->copysize = size;                        	              \
	  return 0;                                                           \
	}                                                                     \
                                                                              \
	ASSERT(info->copysize == size);                                       \
	ASSERT(info->copybuf != NULL);                                        \
                                                                              \
	target = to;                                                          \
	source = info->copybuf;


#define	XLATE_COPYOUT_PROLOGUE(SOURCE_STRUCT, TARGET_STRUCT)		\
	struct SOURCE_STRUCT *source;					\
	struct TARGET_STRUCT *target;					\
									\
	ASSERT(info->smallbuf != NULL);					\
	if ( (sizeof(struct TARGET_STRUCT)) <= info->inbufsize)		\
	    info->copybuf = info->smallbuf;				\
	else								\
	    info->copybuf = kern_malloc(sizeof(struct TARGET_STRUCT));	\
									\
	info->copysize = sizeof(struct TARGET_STRUCT);			\
	target = (struct TARGET_STRUCT *)info->copybuf;			\
	source = (struct SOURCE_STRUCT *)from;


#define	XLATE_COPYOUT_VARYING_PROLOGUE(SOURCE_STRUCT, TARGET_STRUCT, SRC_SIZE) \
	struct SOURCE_STRUCT *source;			\
	struct TARGET_STRUCT *target;			\
	size_t size;					\
							\
	size = SRC_SIZE;				\
	ASSERT(info->smallbuf != NULL);			\
	if (size <= info->inbufsize)			\
	    info->copybuf = info->smallbuf;		\
	else						\
	    info->copybuf = kern_malloc(size);		\
							\
	info->copysize = size;				\
	target = (struct TARGET_STRUCT *)info->copybuf;	\
	source = (struct SOURCE_STRUCT *)from;

#define	XLATE_COPYOUT_ERROR(errno)				\
	{							\
		if (info->copysize <= info->inbufsize)		\
			ASSERT(info->copybuf == info->smallbuf);\
		else						\
			kern_free(info->copybuf);		\
		info->copybuf = NULL;				\
		return (errno);					\
	}


#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL */

#endif /* _SYS_XLATE_H */
