/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * 
 */
/*
 * HISTORY
 * $Log: cs_s.h,v $
 * Revision 65.1  1997/10/20 19:16:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/02/18  22:56:00  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:08  marty]
 *
 * Revision 1.1.4.1  1995/12/08  00:19:01  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1994/12/23  00:25 UTC  kline_s
 * 	Enable RPC I18N functionality
 * 
 * 	HP revision /main/kline_s_mothra_latest/1  1994/12/22  21:36 UTC  kline_s
 * 	Add prototype decl for dce_langinfo().
 * 	[1995/12/07  23:58:34  root]
 * 
 * Revision 1.1.2.5  1994/07/28  18:30:27  mori_m
 * 	CR 11294  Added well-known uuid for codesets attribute.
 * 	[1994/07/28  18:22:16  mori_m]
 * 
 * Revision 1.1.2.4  1994/06/10  20:54:21  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:06  devsrc]
 * 
 * Revision 1.1.2.3  1994/04/12  16:52:01  mori_m
 * 	CR 9701:  Second submission for RPC runtime I18N support.
 * 	[1994/04/12  16:45:01  mori_m]
 * 
 * Revision 1.1.2.2  1994/02/09  21:08:50  mori_m
 * 	CR 9912  Added stub_conversion definition.
 * 	[1994/02/09  21:08:26  mori_m]
 * 
 * Revision 1.1.2.1  1994/02/08  21:33:51  mori_m
 * 	CR 9701:  Initial submission for RPC runtime I18N support
 * 	[1994/02/08  21:33:41  mori_m]
 * 
 * $EndLog$
 */
#if	!defined(_CS_S_H)
#define _CS_S_H
/*
**  NAME
**
**      cs_s.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Data types and definitions for Code Set Interoperability extension
**  (or Internationalization) to RPC runtime.
**
**
*/

#include <langinfo.h>   /* for nl_item type */

/*
 * ISO 10646:1992, UCS-2, Level 2
 * Universal Code Set (encoding) for DCE
 */
#define UCS2_L2		0x0010101

/*
 * When rpc_ns_mgmt_*** routines are extended in the future to, 
 * deal with attributes other than code sets, the new attribute
 * specifier needs to be added here.
 */
#define	RPC_EVAL_TYPE_CODESETS		0x0001
#define	RPC_CUSTOM_EVAL_TYPE_CODESETS	0x0002

/*
 *  Tagged union identifiers.
 *  This is used to distinguish tagged union data structure for
 *  i18n binding handle extension.
 */
#define	RPC_CS_EVAL_TAGS		0
#define RPC_CS_EVAL_METHOD		1


/*
 * Code sets interoperability connection models
 */
#define	RPC_EVAL_NO_CONVERSION		0x0001
#define	RPC_EVAL_RMIR_MODEL		0x0002
#define	RPC_EVAL_CMIR_MODEL		0x0003
#define	RPC_EVAL_SMIR_MODEL		0x0004
#define	RPC_EVAL_INTERMEDIATE_MODEL	0x0005
#define	RPC_EVAL_UNIVERSAL_MODEL	0x0006


/* 
 * Extension to an import context handle.  The new field 'eval_routines'
 * in 'rpc_lkup_rep_t' will be the following data structure.
 */
typedef struct {
	unsigned32		num;
	rpc_cs_eval_list_p	list;
} rpc_cs_eval_func_t, *rpc_cs_eval_func_p_t;


/*
 * R P C _ C S _ C O D E S E T _ I 1 4 Y _ D A T A
 *
 * Argument to OSF code set evaluation routine.  This data will be passed
 * to the evaluation routine, and figure out the compatible client and server
 * code sets combination.  The evaluation routine will be called from a client
 * in OSF implementation, and it will not be called from a client stub.
 *
 * ns_name	: NSI entry name for a server
 * cleanup	: boolean flag indicating any clean-up action required.
 * method_p	: pointer to 'rpc_cs_method_eval_t' data.  See above.
 * status	: result of the code set evaluation.
 */
typedef struct codeset_i14y_data {
	unsigned_char_p_t	ns_name;
	void			*args;
	boolean32		cleanup;
	rpc_cs_method_eval_p_t	method_p;
	error_status_t		status;
} rpc_cs_codeset_i14y_data, *rpc_cs_codeset_i14y_data_p;


/*
 * Internal routine to attach the code set interoperability
 * attributes to a binding handle.  This routine is not intended
 * to be used by application developers.  Only runtime uses it.
 */
extern void rpc_cs_binding_set_method (
#ifdef IDL_PROTOTYPES
    /* [in, out] */ rpc_binding_handle_t *h,
    /* [in] */ rpc_cs_method_eval_p_t method_p,
    /* [out] */ error_status_t *status
#endif
);


/*
 * prototype declarations for locally defined routines
 */
extern void stub_conversion
    _DCE_PROTOTYPE_ ((
	rpc_binding_handle_t	h,
	boolean32		server_side,
	unsigned32		from_tag,
	unsigned32		to_tag,
	byte_t			*conv_ldata,
	unsigned32		conv_l_data_len,
	byte_t			*conv_wdata,
	unsigned32		*conv_p_w_data_len,
	error_status_t		*status
    ));

extern char *dce_langinfo
    _DCE_PROTOTYPE_ ((
	nl_item                 langinfo_val
    ));

/*
 * Well-known UUID for code set attribute
 */
#define rpc_c_uuid_codesets_string	"a1794860-a955-11cd-8443-08000925d3fe"

extern uuid_t rpc_c_attr_real_codesets;
extern uuid_t *rpc_c_attr_codesets;

#endif	/* !defined(_CS_S_H) */
