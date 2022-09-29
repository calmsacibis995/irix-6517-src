/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_VECTOR_H__
#define __SYS_SN_VECTOR_H__

#define NET_VEC_NULL            ((net_vec_t)  0)
#define NET_VEC_BAD             ((net_vec_t) -1)

#ifdef RTL

#define VEC_POLLS_W		16	/* Polls before write times out */
#define VEC_POLLS_R		16	/* Polls before read times out */
#define VEC_POLLS_X		16	/* Polls before exch times out */

#define VEC_RETRIES_W		1	/* Retries before write fails */
#define VEC_RETRIES_R		1	/* Retries before read fails */
#define VEC_RETRIES_X		1	/* Retries before exch fails */

#else /* RTL */

#define VEC_POLLS_W		128	/* Polls before write times out */
#define VEC_POLLS_R		128	/* Polls before read times out */
#define VEC_POLLS_X		128	/* Polls before exch times out */

#define VEC_RETRIES_W		8	/* Retries before write fails */
#define VEC_RETRIES_R           8	/* Retries before read fails */
#define VEC_RETRIES_X		4	/* Retries before exch fails */

#endif /* RTL */


#define NET_ERROR_NONE          0       /* No error             */
#define NET_ERROR_HARDWARE     -1       /* Hardware error       */
#define NET_ERROR_OVERRUN      -2       /* Extra response(s)    */
#define NET_ERROR_REPLY        -3       /* Reply parms mismatch */
#define NET_ERROR_ADDRESS      -4       /* Addr error response  */
#define NET_ERROR_COMMAND      -5       /* Cmd error response   */
#define NET_ERROR_PROT         -6       /* Prot error response  */
#define NET_ERROR_TIMEOUT      -7       /* Too many retries     */
#define NET_ERROR_VECTOR       -8       /* Invalid vector/path  */
#define NET_ERROR_ROUTERLOCK   -9       /* Timeout locking rtr  */
#define NET_ERROR_INVAL	       -10	/* Invalid vector request */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
typedef __uint64_t              net_reg_t;
typedef __uint64_t              net_vec_t;

int             vector_write(net_vec_t dest,
                              int write_id, int address,
                              __uint64_t value);

int             vector_read(net_vec_t dest,
                             int write_id, int address,
                             __uint64_t *value);

int             vector_exch(net_vec_t dest,
                             int write_id, int address,
                             __uint64_t *value);

int             vector_write_node(net_vec_t dest, nasid_t nasid,
                              int write_id, int address,
                              __uint64_t value);

int             vector_read_node(net_vec_t dest, nasid_t nasid,
                             int write_id, int address,
                             __uint64_t *value);

int             vector_exch_node(net_vec_t dest, nasid_t nasid,
                             int write_id, int address,
                             __uint64_t *value);

int             vector_length(net_vec_t vec);
net_vec_t       vector_get(net_vec_t vec, int n);
net_vec_t       vector_prefix(net_vec_t vec, int n);
net_vec_t       vector_modify(net_vec_t entry, int n, int route);
net_vec_t       vector_reverse(net_vec_t vec);
net_vec_t       vector_concat(net_vec_t vec1, net_vec_t vec2);

char		*net_errmsg(int);

#ifndef _STANDALONE
int hub_vector_write(cnodeid_t cnode, net_vec_t vector, int writeid,
	int addr, net_reg_t value);
int hub_vector_read(cnodeid_t cnode, net_vec_t vector, int writeid,
	int addr, net_reg_t *value);
#endif

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#endif /* __SYS_SN_VECTOR_H__ */

