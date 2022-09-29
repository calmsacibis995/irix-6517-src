/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_pkl_krpc.c,v 65.5 1998/03/23 17:27:08 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_pkl_krpc.c,v $
 * Revision 65.5  1998/03/23 17:27:08  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:56  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:55  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:29  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.60.2  1996/02/18  00:00:52  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:54  marty]
 *
 * Revision 1.2  1996/05/29  21:02:44  bhim
 * Defined node_number to be a pointer to a 32-bit value.
 *
 * Revision 1.1  1994/02/02  20:55:43  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.58.2  1994/02/02  20:55:42  cbrooks
 * 	Change argument types via cast/declaration
 * 	[1994/02/02  20:47:53  cbrooks]
 *
 * Revision 1.1.58.1  1994/01/21  22:32:33  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:40  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:58  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:39  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:40:27  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:24  zeliff]
 * 
 * Revision 1.1.2.3  1992/07/17  17:40:49  sommerfeld
 * 	Pick up type uuid pickling fixes.
 * 	[1992/07/16  19:52:53  sommerfeld]
 * 
 * Revision 1.1.2.2  1992/05/28  17:51:39  garyf
 * 	fix compilation warning
 * 	[1992/05/28  17:43:13  garyf]
 * 
 * Revision 1.1  1992/01/19  03:16:40  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**
**  NAME:
**
**      sec_pkl_krpc.c
**
**  FACILITY:
**
**      DCE Security
**
**  ABSTRACT:
**
**      Pickling routines for one sec_ type.
**
**
*/

#ifdef NCK
#undef NCK
#endif
#include <sec_pkl_krpc.h>

#include <dce/pkl.h>
#include <dce/idlbase.h>
#include <dce/rpc.h>
#include <dce/stubbase.h>

/*
 * Ideally, this constant would be expressed in pkl.h but, since
 * pkl.h is derived from pkl.idl, it can't be done.
 */
static rpc_syntax_id_t ndr_syntax_id = {
{0x8a885d04U, 0x1ceb, 0x11c9, 0x9f, 0xe8, {0x8, 0x0, 0x2b, 0x10, 0x48, 0x60}},
2};

extern ndr_format_t ndr_g_local_drep;

uuid_t sec_id_pac_t_typeid = {
    /* d9f3bd98-567d-11ca-9ec6-08001e022936 */
    0xd9f3bd98UL,
    0x567d,
    0x11ca,
    0x9e,
    0xc6,
    {0x08, 0x00, 0x1e, 0x02, 0x29, 0x36}
};

#define PKL_ALIGN_PSP(p, base, alignment)\
    p = base + (((p-base) + (alignment-1)) & ~(alignment-1))

/*
 *
 * PROTOTYPES
 */

static void anon_str_ptr_pickle _DCE_PROTOTYPE_ ((
   idl_char *,
   idl_byte **,
   idl_byte *
));

static void adjust_len_for_string  _DCE_PROTOTYPE_ ((
    unsigned32 *,
    idl_char *
));

static void anon_str_ptr_unpickle  _DCE_PROTOTYPE_ ((
    idl_byte **,
    idl_byte *,
    ndr_format_t,
    void *(*allocator)(size_t),
    idl_char **,
    error_status_t *
));

static void init_ptr_pickling _DCE_PROTOTYPE_ ((void));

static void ptr_pickle _DCE_PROTOTYPE_ ((
   void *,
   idl_char **,
   idl_char *
));

static void sec_id_t_pickle  _DCE_PROTOTYPE_ ((
    sec_id_t *,
    idl_char **,
    idl_char *
));

static void sec_id_tP_pickle  _DCE_PROTOTYPE_ ((
    sec_id_t *,
    idl_char **,
    idl_char *
));

static void ptr_unpickle  _DCE_PROTOTYPE_ ((
    idl_char **,
    idl_char *,
    ndr_format_t ,
    void *(*)(size_t),
    ndr_ulong_int *,
    error_status_t *
));

static void sec_id_t_unpickle _DCE_PROTOTYPE_ ((
    idl_char **,
    idl_char *,
    ndr_format_t ,
    void *(*)(size_t),
    sec_id_t *,
    error_status_t *
));

static void sec_id_tP_unpickle _DCE_PROTOTYPE_ ((
    idl_char **,
    idl_char *,
    ndr_format_t ,
    void *(*)(size_t),
    sec_id_t *,
    error_status_t *
));


/*
 *
 * Function Definitions
 */

static void anon_str_ptr_pickle
#ifdef _DCE_PROTO_
(
    idl_char *data,
    idl_byte **psp,
    idl_byte *bsp
)
#else
(data, psp, bsp)
idl_char *data;
idl_byte **psp;
idl_byte *bsp;
#endif
{
    idl_ulong_int Z;

    if (data != NULL) {
        Z = rpc_ss_strsiz(data, 1);
        PKL_ALIGN_PSP((*psp), bsp, 4);
        rpc_marshall_ulong_int((*psp), Z);
        rpc_advance_mp((*psp), 4);
        rpc_marshall_ulong_int((*psp), 0);
        rpc_advance_mp((*psp), 4);
        rpc_marshall_ulong_int((*psp), Z);
        rpc_advance_mp((*psp), 4);

        for (; Z; Z--)
        {
            rpc_marshall_char((*psp), (*data));
            rpc_advance_mp((*psp), 1);
            data++;
        }
    }
}



static void adjust_len_for_string
#ifdef _DCE_PROTO_
(
    unsigned32 *stream_len,
    idl_char *str
)
#else
(stream_len, str)
unsigned32 *stream_len;
idl_char *str;
#endif
{
    if (str != NULL)
    {
        PKL_ALIGN_PSP((*stream_len), 0, 4);
        *stream_len += 12 + rpc_ss_strsiz(str, 1);
    }
}

static void anon_str_ptr_unpickle
#ifdef _DCE_PROTO_
(
    idl_byte ** psp,
    idl_byte * bsp,
    ndr_format_t drep,
    void *(*allocator)(size_t),
    idl_char **data,
    error_status_t *st
)
#else
(psp, bsp, drep, allocator, data, st)
idl_byte **psp;
idl_byte *bsp;
ndr_format_t drep;
void *(*allocator)(size_t);
idl_char **data;
error_status_t *st;
#endif
{
    idl_ulong_int B;
    idl_ulong_int A;
    idl_ulong_int Z;
    idl_char *element;
#ifdef SGIMIPS
    unsigned32 node_number;
#else
    unsigned long node_number;
#endif /* SGIMIPS */

#ifdef SGIMIPS
    node_number = *(unsigned32 *)data;
#else
    node_number = (unsigned long)*data;
#endif /* SGIMIPS */
    if (node_number == 0) {
      *data = NULL; /* gratuitous hyper-correctness in case NULL != 0 */
      return;
    }
    PKL_ALIGN_PSP((*psp), bsp, 4);
    rpc_convert_ulong_int(drep, ndr_g_local_drep, (*psp), Z);
    rpc_advance_mp((*psp), 4);
    *data = (idl_char (*))(*allocator)((Z==0) ? 1 : Z);
    if (*data == NULL)
    {
        *st = rpc_s_no_memory;
        return;
    }

    rpc_convert_ulong_int(drep, ndr_g_local_drep, (*psp), A);
    rpc_advance_mp((*psp), 4);
    rpc_convert_ulong_int(drep, ndr_g_local_drep, (*psp), B);
    rpc_advance_mp((*psp), 4);
    if (B > Z)
    {
        *st = rpc_s_fault_invalid_bound;
        return;
    }
    element = *data;
    while (B)
    {
        rpc_convert_char(drep, ndr_g_local_drep, (*psp), (*element));
        rpc_advance_mp((*psp), 1);
        element++;
        B--;
    }
}

static unsigned32 next_non_null_node_number = 0 ;

static void init_ptr_pickling(void)
{
    next_non_null_node_number = 1;
}

static void ptr_pickle
#ifdef _DCE_PROTO_
(
    void *data,
    idl_char **psp,
    idl_char *bsp
)
#else
(data, psp, bsp)
void *data;
idl_char **psp;
idl_char *bsp;
#endif
{
    PKL_ALIGN_PSP((*psp), bsp, 4);
    if (data == NULL)
        rpc_marshall_ulong_int((*psp), 0);
    else
    {
        rpc_marshall_ulong_int((*psp), next_non_null_node_number);
        next_non_null_node_number++;
    }
    rpc_advance_mp((*psp), 4);
}

static void sec_id_t_pickle
#ifdef _DCE_PROTO_
(
    sec_id_t *data,
    idl_char **psp,
    idl_char *bsp
)
#else
(data, psp, bsp)
sec_id_t *data;
idl_char **psp;
idl_char *bsp;
#endif
{
    idl_char *element;
    idl_ulong_int index;

    PKL_ALIGN_PSP((*psp), bsp, 4);
    rpc_marshall_ulong_int((*psp), data->uuid.time_low);
    rpc_advance_mp((*psp), 4);
    rpc_marshall_ushort_int((*psp), data->uuid.time_mid);
    rpc_advance_mp((*psp), 2);
    rpc_marshall_ushort_int((*psp), data->uuid.time_hi_and_version);
    rpc_advance_mp((*psp), 2);
    rpc_marshall_usmall_int((*psp), data->uuid.clock_seq_hi_and_reserved);
    rpc_advance_mp((*psp), 1);
    rpc_marshall_usmall_int((*psp), data->uuid.clock_seq_low);
    rpc_advance_mp((*psp), 1);
    element = &data->uuid.node[0];
    for (index = 6; index; index--)
    {
        rpc_marshall_byte((*psp), (*element));
        rpc_advance_mp((*psp), 1);
        element++;
    }
    ptr_pickle(data->name, psp, bsp);
}

static void sec_id_tP_pickle
#ifdef _DCE_PROTO_
(
    sec_id_t *data,
    idl_char **psp,
    idl_char *bsp
)
#else
(data, psp, bsp)
sec_id_t *data;
idl_char **psp;
idl_char *bsp;
#endif
{
    anon_str_ptr_pickle(
        (idl_char *) data->name,
        psp, bsp);
}

static void ptr_unpickle
#ifdef _DCE_PROTO_
(
    idl_char **psp,
    idl_char *bsp,
    ndr_format_t drep,
    void *(*allocator)(size_t),
    ndr_ulong_int *data,
    error_status_t *st
)
#else
(psp, bsp, drep, allocator, data, st)
idl_char **psp;
idl_char *bsp;
ndr_format_t drep;
void *(*allocator)(size_t);
ndr_ulong_int *data;
error_status_t *st;
#endif
{
    PKL_ALIGN_PSP((*psp), bsp, 4);
    rpc_convert_ulong_int(drep, ndr_g_local_drep, (*psp), (*data));
    rpc_advance_mp((*psp), 4);
}

static void sec_id_t_unpickle
#ifdef _DCE_PROTO_
(
    idl_char **psp,
    idl_char *bsp,
    ndr_format_t drep,
    void *(*allocator)(size_t),
    sec_id_t *data,
    error_status_t *st
)
#else 
(psp, bsp, drep, allocator, data, st)
idl_char **psp;
idl_char *bsp;
ndr_format_t drep;
void *(*allocator)(size_t);
sec_id_t *data;
error_status_t *st;
#endif
{
    idl_char *element;
    idl_ulong_int index;

    PKL_ALIGN_PSP((*psp), bsp, 4);
    rpc_convert_ulong_int(drep, ndr_g_local_drep, (*psp), data->uuid.time_low);
    rpc_advance_mp((*psp), 4);
    rpc_convert_ushort_int(drep, ndr_g_local_drep, (*psp), data->uuid.time_mid);
    rpc_advance_mp((*psp), 2);
    rpc_convert_ushort_int(drep, ndr_g_local_drep, (*psp), data->uuid.time_hi_and_version);
    rpc_advance_mp((*psp), 2);
    rpc_convert_usmall_int(drep, ndr_g_local_drep, (*psp), data->uuid.clock_seq_hi_and_reserved);
    rpc_advance_mp((*psp), 1);
    rpc_convert_usmall_int(drep, ndr_g_local_drep, (*psp), data->uuid.clock_seq_low);
    rpc_advance_mp((*psp), 1);
    element = &data->uuid.node[0];
    for (index = 6; index; index--)
    {
        rpc_convert_byte(drep, ndr_g_local_drep, (*psp), (*element));
        rpc_advance_mp((*psp), 1);
        element++;
    }
    ptr_unpickle(psp, bsp, drep, allocator, (idl_ulong_int *)&data->name, st);
}

static void sec_id_tP_unpickle
#ifdef _DCE_PROTO_
(
    idl_char **psp,
    idl_char *bsp,
    ndr_format_t drep,
    void *(*allocator)(size_t),
    sec_id_t *data,
    error_status_t *st
)
#else 
(psp, bsp, drep, allocator, data, st)
idl_char **psp;
idl_char *bsp;
ndr_format_t drep;
void *(*allocator)(size_t);
sec_id_t *data;
error_status_t *st;
#endif 
{
    anon_str_ptr_unpickle(
        psp, bsp, drep,
        allocator,
        &data->name, st);
}

/*===================================================================*/

void sec_id_pac_t_pickle
#ifdef _DCE_PROTO_
(
    sec_id_pac_t *data,
    rpc_syntax_id_t *syntax,
    void *(*allocator)(size_t),
    unsigned32 alloc_mod,
    idl_pkl_t **pickle,
    unsigned32 *pickle_len,
    error_status_t *st
)
#else 
(data, syntax, allocator, alloc_mod, pickle, pickle_len, st)
    sec_id_pac_t *data;
    rpc_syntax_id_t *syntax;
    void *(*allocator)(size_t);
    unsigned32 alloc_mod;
    idl_pkl_t **pickle;
    unsigned32 *pickle_len;
    error_status_t *st;
#endif
{
    unsigned32 stream_len = 8; /* ndr format label and fill bytes */
    idl_byte *psp, *bsp; /* pickle data pointers */
    idl_byte *bp;
    unsigned16 index;
    sec_id_t *groups_element;
    sec_id_foreign_t *foreign_groups_element;

    *st = error_status_ok;

    stream_len +=  2  /* data->pac_type (enum) */
                 + 2  /* gap */
                 + 4  /* data->authenticated */
                 + 20 /* data->realm (uuid and node #) */
                 + 20 /* data->principal (uuid and node #) */
                 + 20 /* data->group (uuid and node #) */
                 + 2  /* data->num_groups */
                 + 2  /* data->num_foreign_groups */
                 + 4  /* data->groups node number */
                 + 4; /* data->foreign groups node number */

    adjust_len_for_string(&stream_len, data->realm.name);
    adjust_len_for_string(&stream_len, data->principal.name);
    adjust_len_for_string(&stream_len, data->group.name);

    if (data->groups != NULL)
    {
        PKL_ALIGN_PSP(stream_len, 0, 4);
        stream_len += 4; /* Z for *groups */
        stream_len += data->num_groups * 20; /* uuid and node # */
        for (index = 0; index < data->num_groups; index++)
            adjust_len_for_string(
                &stream_len,
                data->groups[index].name);
    }
    if (data->foreign_groups != NULL)
    {
        PKL_ALIGN_PSP(stream_len, 0, 4);
        stream_len += 4; /* Z for *foreign_groups */
        stream_len += data->num_foreign_groups * 2 * 20;
        for (index = 0; index < data->num_foreign_groups; index++)
        {
            adjust_len_for_string(
                &stream_len,
                data->foreign_groups[index].id.name);
            adjust_len_for_string(
                &stream_len,
                data->foreign_groups[index].realm.name);
        }
    }

    *pickle_len = stream_len + idl_pkl_data_offset;
    *pickle_len = (*pickle_len + (alloc_mod-1)) & ~(alloc_mod-1);
    *pickle = (*allocator)(*pickle_len);

    /* ought to check input syntax, but ndr is currently the only choice */

    idl_pkl_set_header (**pickle, 0, stream_len, &ndr_syntax_id, &sec_id_pac_t_typeid);

    psp = bsp = **pickle + idl_pkl_data_offset;

    init_ptr_pickling();

    /*
     * This code represents an architectural decision:
     *
     *    In a byte stream in an (NDR-flavored) pickle the first
     *    four bytes are the format label and the next four bytes are
     *    0.  The pickled data starts at byte 8 of the data part.
     */
    *(ndr_format_t *)psp = ndr_g_local_drep;
    rpc_advance_mp(psp, 4);
    rpc_marshall_ulong_int(psp, 0);
    rpc_advance_mp(psp, 4);


    rpc_marshall_enum(psp, data->pac_type);
    rpc_advance_mp(psp, 2);
    PKL_ALIGN_PSP(psp, bsp, 4);
    rpc_marshall_ulong_int(psp, data->authenticated);
    rpc_advance_mp(psp, 4);

    sec_id_t_pickle(&data->realm, &psp, bsp);
    sec_id_t_pickle(&data->principal, &psp, bsp);
    sec_id_t_pickle(&data->group, &psp, bsp);
 
    rpc_marshall_ushort_int(psp, (data->num_groups));
    rpc_advance_mp(psp, 2);
    rpc_marshall_ushort_int(psp, (data->num_foreign_groups));
    rpc_advance_mp(psp, 2);

    ptr_pickle(data->groups, &psp, bsp);
    ptr_pickle(data->foreign_groups, &psp, bsp);

    sec_id_tP_pickle(&data->realm, &psp, bsp);
    sec_id_tP_pickle(&data->principal, &psp, bsp);
    sec_id_tP_pickle(&data->group, &psp, bsp);

    if (data->groups != NULL)
    {
        PKL_ALIGN_PSP(psp, bsp, 4);
        rpc_marshall_ulong_int(psp, data->num_groups);
        rpc_advance_mp(psp, 4);
        groups_element = data->groups;
        for (index = data->num_groups; index; index--)
        {
            sec_id_t_pickle(groups_element, &psp, bsp);
            groups_element++;
        }
        groups_element = data->groups;
        for (index = data->num_groups; index; index--)
        {
            sec_id_tP_pickle(groups_element, &psp, bsp);
            groups_element++;
        }
    }

    if (data->foreign_groups != NULL)
    {
        PKL_ALIGN_PSP(psp, bsp, 4);
        rpc_marshall_ulong_int(psp, data->num_foreign_groups);
        rpc_advance_mp(psp, 4);
        foreign_groups_element = data->foreign_groups;
        for (index = data->num_foreign_groups; index; index--)
        {
            sec_id_t_pickle(&foreign_groups_element->id, &psp, bsp);
            sec_id_t_pickle(&foreign_groups_element->realm, &psp, bsp);
            foreign_groups_element++;
        }
        foreign_groups_element = data->foreign_groups;
        for (index = data->num_foreign_groups; index; index--)
        {
            sec_id_tP_pickle(&foreign_groups_element->id, &psp, bsp);
            sec_id_tP_pickle(&foreign_groups_element->realm, &psp, bsp);
            foreign_groups_element++;
        }
    }
}

void sec_id_pac_t_unpickle
#ifdef _DCE_PROTO_
(
    idl_pkl_t *pickle,
    void *(*allocator)(size_t),
    sec_id_pac_t *data,
    error_status_t *st
)
#else 
(pickle, allocator, data, st)
idl_pkl_t *pickle;
void *(*allocator)(size_t);
sec_id_pac_t *data;
error_status_t *st;
#endif
{
    idl_byte *psp, *bsp;
    ndr_format_t drep;
    idl_ulong_int Z;
    unsigned16 index;
    sec_id_t *groups_element;
    sec_id_foreign_t *foreign_groups_element;
    uuid_t typeid;

    idl_pkl_get_type_uuid (*pickle, &typeid);
    if (!uuid_equal(&sec_id_pac_t_typeid, &typeid, st))
    {
        *st = rpc_s_wrong_pickle_type;
        return;
    }

    *st = error_status_ok;

    psp = bsp = *pickle + idl_pkl_data_offset;
    drep = *(ndr_format_t *)psp;
    rpc_advance_mp(psp, 8);

    rpc_convert_enum(drep, ndr_g_local_drep, psp, data->pac_type);
    rpc_advance_mp(psp, 2);
    PKL_ALIGN_PSP(psp, bsp, 4);
    rpc_convert_ulong_int(drep, ndr_g_local_drep, psp, data->authenticated);
    rpc_advance_mp(psp, 4);

    sec_id_t_unpickle(&psp, bsp, drep, allocator, &data->realm, st);
    sec_id_t_unpickle(&psp, bsp, drep, allocator, &data->principal, st);
    sec_id_t_unpickle(&psp, bsp, drep, allocator, &data->group, st);

    rpc_convert_ushort_int(drep, ndr_g_local_drep, psp, (data->num_groups));
    rpc_advance_mp(psp, 2);
    rpc_convert_ushort_int(drep, ndr_g_local_drep, psp, (data->num_foreign_groups));
    rpc_advance_mp(psp, 2);

    ptr_unpickle(&psp, bsp, drep, allocator, (idl_ulong_int *)&data->groups, st);
    ptr_unpickle(&psp, bsp, drep, allocator, (idl_ulong_int *)&data->foreign_groups, st);

    sec_id_tP_unpickle(&psp, bsp, drep, allocator, &data->realm, st);
    sec_id_tP_unpickle(&psp, bsp, drep, allocator, &data->principal, st);
    sec_id_tP_unpickle(&psp, bsp, drep, allocator, &data->group, st);

    if (data->groups != NULL)
    {
        PKL_ALIGN_PSP(psp, bsp, 4);
        rpc_convert_ulong_int(drep, ndr_g_local_drep, psp, Z);
        rpc_advance_mp(psp, 4);

        if (Z <= 0) 
        {
            data->groups = (*allocator)(1);
        } 
        else 
        {
            data->groups = (*allocator)(Z*sizeof(sec_id_t));
            groups_element = data->groups;
            for (index = Z; index; index--)
            {
                sec_id_t_unpickle(&psp, bsp, drep, allocator, groups_element, st);
                groups_element++;
            }
            groups_element = data->groups;
            for (index = Z; index; index--)
            {
                sec_id_tP_unpickle(&psp, bsp, drep, allocator, groups_element, st);
                groups_element++;
            }
        }
    }

    if (data->foreign_groups != NULL)
    {
        PKL_ALIGN_PSP(psp, bsp, 4);
        rpc_convert_ulong_int(drep, ndr_g_local_drep, psp, Z);
        rpc_advance_mp(psp, 4);
        if (Z <= 0) {
            data->foreign_groups = (*allocator)(1);
        } else {
            data->foreign_groups = (*allocator)(Z*sizeof(sec_id_foreign_t));
            foreign_groups_element = data->foreign_groups;
            for (index = Z; index; index--)
            {
                sec_id_t_unpickle(
                    &psp, bsp, drep, allocator,
                    &foreign_groups_element->id, st);
                sec_id_t_unpickle(
                    &psp, bsp, drep, allocator,
                    &foreign_groups_element->realm, st);
                foreign_groups_element++;
            }
            foreign_groups_element = data->foreign_groups;
            for (index = Z; index; index--)
            {
                sec_id_tP_unpickle(
                    &psp, bsp, drep, allocator,
                    &foreign_groups_element->id, st);
                sec_id_tP_unpickle(
                    &psp, bsp, drep, allocator,
                    &foreign_groups_element->realm, st);
                foreign_groups_element++;
            }
        }
    }
}
