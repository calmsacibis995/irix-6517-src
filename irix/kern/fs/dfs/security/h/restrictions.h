/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: restrictions.h,v $
 * Revision 65.1  1997/10/20 19:46:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/02/18  22:58:11  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:17:02  marty]
 *
 * Revision 1.1.4.1  1995/12/08  17:27:10  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:53:22  root]
 * 
 * Revision 1.1.2.2  1994/10/06  20:28:38  agd
 * 	expand copyright
 * 	[1994/10/06  14:27:36  agd]
 * 
 * Revision 1.1.2.1  1994/08/04  16:12:43  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/28  17:19:19  mdf]
 * 
 * $EndLog$
 */

/* restrictions.h
 * 
 * Copyright (c) Hewlett-Packard Company 1993
 * Unpublished work. All Rights Reserved.
 */

#ifndef _RESTRICTIONS_H
#define _RESTRICTIONS_H

extern boolean32 sec_id_any_other_rstr_in (
    sec_id_restriction_set_t *rstrs
);

extern void sec_id_get_anonymous_epac (
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_data_t    *epac,         /* [in/out] */
    error_status_t        *stp           /* [out] */
);

extern boolean32 sec_id_is_anonymous_epac (
    sec_id_epac_data_t    *epac,         /* [in] */
    error_status_t        *stp           /* [out] */
);

extern boolean32 sec_id_is_anonymous_pa (
    sec_id_pa_t           *pa,         /* [in] */
    error_status_t        *stp           /* [out] */
);

extern void sec_id_restriction_intersection (
    idl_void_p_t                (*alloc)(idl_size_t size),     /* [in] */
    void                        (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_restriction_set_t    *rs1,    /* [in] epac's opt_req restriction */
    uuid_t                      *cell1_uuid,
    sec_id_restriction_set_t    *rs2,    /* [in] epac's opt_req restriction */
    uuid_t                      *cell2_uuid,
    sec_id_restriction_set_t    **rs3,   /* [out] epac's restriction */
    error_status_t              *st      /* [out] */
);

extern void sec_id_opt_req_restriction_union (
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_opt_req_t    *rs1,    /* [in] epac's restriction */
    sec_id_opt_req_t    *rs2,    /* [in] epac's restriction */
    sec_id_opt_req_t    **rs3,   /* [out] epac's opt_req restriction */
    error_status_t      *st      /* [out] */
);

extern boolean32 sec_id_check_restrictions (
    sec_id_pa_t                 *epa,    /* [in] epac's pa */
    sec_id_restriction_set_t    *ers,    /* [in] epac's restriction */
    sec_id_pa_t                 *spa,     /* [in] server's pa */
    error_status_t              *st     /* [out] */
);

#endif /* _RESTRICTIONS_H */
