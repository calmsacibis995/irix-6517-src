/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: idl_es.h,v $
 * Revision 65.1  1997/10/20 19:16:17  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.340.2  1996/02/18  22:57:18  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:12  marty]
 *
 * Revision 1.1.340.1  1995/12/08  00:23:09  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:10 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/08  00:01:00  root]
 * 
 * Revision 1.1.336.4  1994/06/22  20:41:37  tom
 * 	Bug 10979 - Make idl_es_handle_t a true opaque type.
 * 	[1994/06/22  20:41:17  tom]
 * 
 * Revision 1.1.336.2  1994/05/10  18:03:24  tom
 * 	Add get/set encoding service attribute interfaces.
 * 	[1994/05/10  17:35:03  tom]
 * 
 * Revision 1.1.336.1  1994/01/21  22:39:57  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:30:49  cbrooks]
 * 
 * Revision 1.1.2.2  1993/07/07  20:12:05  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:40:19  ganni]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) Digital Equipment Corporation, 1992, 1993
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**  
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of 
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or 
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**  NAME:
**
**      idl_es.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      User routine protoypes for IDL encoding services
**
*/

#ifndef IDL_ES_H
#define IDL_ES_H	1

#include <dce/dce.h>
#include <dce/rpc.h>

/*****************************************************************************/
/*                                                                           */
/*  IDL encoding services                                                    */
/*                                                                           */
/*****************************************************************************/

typedef struct idl_es_handle_s_t *idl_es_handle_t;

/*
** Data types for read/write/alloc routines that manage
** incremental processing of the encoding.
*/

#ifdef __cplusplus 
extern "C"  {
#endif /* __cplusplus */


typedef void (IDL_ENTRY *idl_es_allocate_fn_t) _DCE_PROTOTYPE_ ( 
  (
    idl_void_p_t    ,  /* [in,out] user state */
    idl_byte	    **,   /* [out] Address of buffer */
    idl_ulong_int   *   /* [in,out] Requested Size/Size of buffer */
  )
);/* Routine to allocate a buffer */

typedef void (IDL_ENTRY *idl_es_write_fn_t)  _DCE_PROTOTYPE_ ( 
  (
    idl_void_p_t    ,  /* [in,out] user state */ 
    idl_byte	    *,   /* [in] Encoded data */
    idl_ulong_int       /* [in] Size of encoded data */
  )
);   /* Routine to write encoded data */

typedef void (IDL_ENTRY *idl_es_read_fn_t)  _DCE_PROTOTYPE_ ( 
  (
    idl_void_p_t ,	    /* [in,out] user state */ 
    idl_byte **,          /* [out] Data to be decoded */
    idl_ulong_int *     /* [out] Size of buf */
  )
);    /* Routine to read encoded data */

/*
** Provides an idl_es_handle_t which allows an encoding operation to be
** performed in an incremental fashion similar to pipe processing.
** Buffers areas are requested by the stubs via the allocate routine as
** necessary and when filled, are provided to the application via a
** call to the write routine.  This routine is suitable for encodings
** that can be incrementally processed by the applications such as when
** they are written to a file.  The application may invoke multiple
** operations utilizing the same handle.
*/

void IDL_ENTRY idl_es_encode_incremental  _DCE_PROTOTYPE_ ( 
  (
    idl_void_p_t	    ,  /* [in] user state */
    idl_es_allocate_fn_t    ,  /* [in] alloc routine */
    idl_es_write_fn_t	    ,  /* [in] write routine */
    idl_es_handle_t	    *,	    /* [out] encoding handle */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Provides an idl_es_handle_t which allows an encoding operation to be
** performed into a buffer provided by the application.  If the buffer
** is not of sufficient size to receive the encoding the error
** rpc_s_no_memory is reported.  This mechanism provides a simple, but
** high performance encoding mechanism provided that the upper limit of
** the encoding size is known.  The application may invoke multiple
** operation utilizing the same handle.
*/
void IDL_ENTRY idl_es_encode_fixed_buffer  _DCE_PROTOTYPE_ ( 
  (
    idl_byte		    *,    /* [in] pointer to buffer to    */
				    /* receive the encoding (must   */
				    /* be 8-byte aligned).	    */
    idl_ulong_int	    ,  /* [in] size of buffer provided */
    idl_ulong_int	    *, /* [out] size of the resulting  */
				    /* encoding	(set after	    */
				    /* execution of the stub)	    */
    idl_es_handle_t	    *,	    /* [out] encoding handle */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Provides and idl_es_handle_t which provides an encoding in a
** stub-allocated buffer.  Although this mechanism provides the
** simplest application interface, large encodings may incur a
** performance penalty for this convenience.  The return buffer is
** allocated via the client memory allocation mechanism currently in
** effect at the time of the encoding call.
*/
void IDL_ENTRY idl_es_encode_dyn_buffer  _DCE_PROTOTYPE_ ( 
  (
    idl_byte		    **,   /* [out] pointer to recieve the */
				    /* dynamically allocated buffer */
				    /* which contains the encoding  */
    idl_ulong_int	    *, /* [out] size of the resulting  */
				    /* encoding (set after	    */
				    /* execution of the stub)	    */
    idl_es_handle_t	    *,	    /* [out] decoding handle */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Provides an idl_es_handle_t which allows an decoding operation to
** be performed in an incremental fashion similar to pipe processing.
** Buffers containing portions of the encoding are provided by the
** application via calls to the read routine.  This routine is
** suitable for encodings that can be incremental decoded by the
** stubs such as when they are read from a file.
*/
void IDL_ENTRY idl_es_decode_incremental  _DCE_PROTOTYPE_ ( 
  (
    idl_void_p_t	    ,  /* [in] user state */
    idl_es_read_fn_t	    ,   /* [in] routine to supply buffers */
    idl_es_handle_t	    *,	    /* [out] decoding handle */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Provides an idl_es_handle_t which allows an decoding operation to
** be performed from a buffer containing the encoded data.  There may
** be a performance penalty if the buffer provided is not 8-byte
** aligned.
*/
void IDL_ENTRY idl_es_decode_buffer  _DCE_PROTOTYPE_ ( 
  (
    idl_byte		    *,    /* [in] pointer to buffer	    */
				    /* containing the encoding	    */
    idl_ulong_int	    ,   /* [in] size of buffer provided */
    idl_es_handle_t	    *,	    /* [out] decoding handle */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Returns the rpc_if_id_t and operation number contained within the
** encoding associated with an idl_es_handle_t.  This information is
** available during a decode operation, and after the operation has
** been invoked for an encode operations.  If this information is not
** available, the status rpc_s_unknown_if is returned.
*/
void IDL_ENTRY idl_es_inq_encoding_id  _DCE_PROTOTYPE_ ( 
  (
    idl_es_handle_t	    ,	    /* [in] decoding handle */
    rpc_if_id_t		    *, /* [out] RPC interface	    */
				    /* identifier (including	    */
				    /* version information)	    */
    idl_ulong_int	    *,    /* [out] operation number */
    error_status_t	    *	    /* [out] status */
  )
);

/*
** Frees a idl_es_handle_t and its associated resources
*/
void IDL_ENTRY idl_es_handle_free _DCE_PROTOTYPE_ (
  (
    idl_es_handle_t	*,	    /* [in,out] handle to free */
    error_status_t	*	    /* [out] status */
  )
);

/*
 *  Machinery for stubs which support encodings in more than one transfer
 *  syntax
 */
typedef enum {idl_es_transfer_syntax_ndr 
}idl_es_transfer_syntax_t;

void IDL_ENTRY idl_es_set_transfer_syntax  _DCE_PROTOTYPE_ ( 
  (
    idl_es_handle_t ,
    idl_es_transfer_syntax_t ,
    error_status_t *
  )
);

/*
 * Routines for getting and setting the attribute flag.
 * 
 * Flags:
 * IDL_ES_NO_ENCODING_CHECK     This tells the encoding services to not
 *                              check the interface id so an interface that
 * 				did not encode data can decode parts of it
 *				(such as a common header).
 */

#define IDL_ES_NO_ENCODING_CHECK	0x1

void idl_es_set_attrs(
    idl_es_handle_t,	/* [in] handle */
    unsigned32,		/* [in] flags */
    error_status_t *);	/* [out] status */

void idl_es_inq_attrs(
    idl_es_handle_t,	/* [in] handle */
    unsigned32 *,	/* [out] flags */
    error_status_t *);	/* [out] status */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IDL_ES_H */

