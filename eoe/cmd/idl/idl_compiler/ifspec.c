/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ifspec.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:54  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:41  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:46  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:56  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:19  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      ifspec.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines for the emission of ifspecs
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <ifspec.h>
#include <bedeck.h>
#include <dutils.h>

/******************************************************************************/
/*                                                                            */
/*    Return a pointer to the name of the if_spec                             */
/*                                                                            */
/******************************************************************************/
static char *BE_ifspec_name
#ifdef PROTO
(
    AST_interface_n_t *ifp,
    BE_output_k_t kind
)
#else
(ifp, kind)
    AST_interface_n_t *ifp;
    BE_output_k_t kind;
#endif
{
    static char retval[100];

    sprintf(retval, "%s_v%d_%d_%c_ifspec", BE_get_name(ifp->name),
            (ifp->version%65536), (ifp->version/65536),
            kind == BE_client_stub_k ? 'c' : 's');

    return retval;
}

/******************************************************************************/
/*                                                                            */
/*    Spell the manager epv to the output stream                              */
/*                                                                            */
/******************************************************************************/
static void CSPELL_manager_epv
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *ifp
)
#else
( fid, ifp )
    FILE *fid;
    AST_interface_n_t *ifp;
#endif
{
    AST_export_n_t *p_export;
    boolean first_op = true;

    fprintf( fid, "\n static %s_v%d_%d_epv_t NIDL_manager_epv = {\n",
           BE_get_name(ifp->name), (ifp->version%65536), (ifp->version/65536) );

    for( p_export = ifp->exports; p_export != NULL; p_export = p_export->next )
    {
        if (p_export->kind == AST_operation_k)
        {
            if ( ! first_op ) fprintf( fid, "," );
            spell_name( fid, p_export->thing_p.exported_operation->name );
            fprintf( fid, "\n" );
            first_op = false;
        }
    }

    fprintf( fid,"};\n" );
}


/******************************************************************************/
/*                                                                            */
/*    Spell an interface definition and related material to the output stream */
/*                                                                            */
/******************************************************************************/
void CSPELL_interface_def
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *ifp,
    BE_output_k_t kind,
    boolean generate_mepv
)
#else
(fid, ifp, kind, generate_mepv)
    FILE *fid;
    AST_interface_n_t *ifp;
    BE_output_k_t kind;
    boolean generate_mepv;
#endif
{
    long        family;
    boolean     first;
    long        i, endpoints;
    char *protseq, *portspec;

    if (endpoints = ifp->number_of_ports)
    {
        first = true;
        fprintf(fid,
                "static rpc_endpoint_vector_elt_t NIDL_endpoints[%d] = \n{",
                endpoints);
        for (i = 0; i < endpoints; i++)
        {
            STRTAB_str_to_string(ifp->protocol[i], &protseq);
            STRTAB_str_to_string(ifp->endpoints[i], &portspec);
            if (!first) fprintf(fid, ",\n");
            fprintf(fid,
               "{(unsigned_char_p_t)\042%s\042, (unsigned_char_p_t)\042%s\042}",
                   protseq, portspec);
            first = false;
        }
        fprintf(fid, "};\n");
    }

    /* Manager epv if required */
    if (generate_mepv)
    {
        CSPELL_manager_epv( fid, ifp );
    }

    /* Transfer syntax */
    fprintf(fid, "\nstatic rpc_syntax_id_t NIDL_ndr_id = {\n");
    fprintf(fid,
"{0x8a885d04, 0x1ceb, 0x11c9, 0x9f, 0xe8, {0x8, 0x0, 0x2b, 0x10, 0x48, 0x60}},");
    fprintf(fid, "\n2};\n");

    fprintf(fid, "\nstatic rpc_if_rep_t NIDL_ifspec = {\n");
    fprintf(fid, "  1, /* ifspec rep version */\n");
    fprintf(fid, "  %d, /* op count */\n", ifp->op_count);
    fprintf(fid, "  %d, /* if version */\n", ifp->version);
    fprintf(fid, "  {");
    fprintf(fid, "0x%08.8lx, ", ifp->uuid.time_low);
    fprintf(fid, "0x%04.4x, ", ifp->uuid.time_mid);
    fprintf(fid, "0x%04.4x, ", ifp->uuid.time_hi_and_version);
    fprintf(fid, "0x%02.2x, ", ifp->uuid.clock_seq_hi_and_reserved);
    fprintf(fid, "0x%02.2x, ", ifp->uuid.clock_seq_low);
    fprintf(fid, "{");
    first = true;
    for (i = 0; i < 6; i++)
    {
        if (first)
            first = false;
        else
            fprintf (fid, ", ");
        fprintf (fid, "0x%x", ifp->uuid.node[i]);
    }
    fprintf(fid, "}},\n");
    fprintf(fid, "  2, /* stub/rt if version */\n");
    fprintf(fid, "  {%d, %s}, /* endpoint vector */\n", endpoints,
                 endpoints ? "NIDL_endpoints" : "NULL");
    fprintf(fid, "  {1, &NIDL_ndr_id} /* syntax vector */\n");
    if (kind == BE_server_stub_k)
    {
        fprintf(fid, ",NIDL_epva /* server_epv */\n");
        if (generate_mepv)
        {
            fprintf(fid,",(rpc_mgr_epv_t)&NIDL_manager_epv /* manager epv */\n");
        }
        else
        {
            fprintf(fid,",NULL /* manager epv */\n");
        }
    }
    fprintf(fid, "};\n");

    fprintf(fid,
            "/* global */ rpc_if_handle_t %s = (rpc_if_handle_t)&NIDL_ifspec;\n",
            BE_ifspec_name(ifp, kind));
}
