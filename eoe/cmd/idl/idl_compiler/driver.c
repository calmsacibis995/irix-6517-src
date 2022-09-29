/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: driver.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.5.4  1993/01/03  21:39:04  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:20  bbelch]
 *
 * Revision 1.1.5.3  1992/12/23  18:45:49  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:33  zeliff]
 * 
 * Revision 1.1.5.2  1992/09/29  20:40:53  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:34:49  weisman]
 * 
 * Revision 1.1.2.2  1992/05/05  15:54:38  harrow
 * 	*** empty log message ***
 * 
 * Revision 1.1.1.2  1992/05/05  13:56:31  harrow
 * 	Enhance -v to output full cc compile command issued.
 * 
 * Revision 1.1  1992/01/19  03:01:02  devrcs
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
**      driver.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Main "driver" module - Invokes each major compiler component.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>       /* IDL common defs */
#include <signal.h>
#include <setjmp.h>
#include <driver.h>

#include <ast.h>        /* Abstract Syntax Tree defs */
#include <command.h>    /* Command Line defs */
#include <files.h>      /* File handling defs */
#include <backend.h>    /* Decl. of BE_main() */
#include <message.h>    /* reporting functions */
#include <frontend.h>   /* Declaration of FE_main() */

/* Macro to call a function and return if boolean return status is FALSE */
#define CALL(rtn) \
    { \
    status = rtn; \
    if (!status) \
        {nidl_terminate();} \
    }

/*
 * setjmp/longjmp buffer to use for termination during fatal errors
 */
#if defined(SNI_SVR4_POSIX)
static sigjmp_buf nidl_termination_jmp_buf;
#else
static jmp_buf nidl_termination_jmp_buf;
#endif 

/*
** n i d l _ t e r m i n a t e
**
** This routine utilizes setjmp/longjmp to perform an orderly termination
** of the idl compiler.
*/
void nidl_terminate (
#ifdef PROTO
void
#endif
)
{
#if defined(SNI_SVR4_POSIX)
    siglongjmp(nidl_termination_jmp_buf, 1);
#else
    longjmp(nidl_termination_jmp_buf,1);
#endif
}



/*
** a t t e m p t _ t o _ p r i n t _ e r r o r s
**
** This routine is established as an exception handler with the base system
** such that we always attempt to print out error messages even in the case of
** an error in the compiler.  The print_errors routine, which we call is coded
** such that we cannot get into an infinite loop.
*/
static long attempt_to_print_errors()
{
#ifndef vms
# if defined(SNI_SVR4_POSIX)
	/*
	*	It is not necessary to reset the handler for the signal to
	*	SIG_DFL, this is taken care of by sigaction.  In addition,
	*	the signal is not blocked on entry to this call, so 
	*	it is not necessary to change the signal mask for the 
	*	process.
	*/
# else
    signal(SIGBUS,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    signal(SIGFPE,SIG_DFL);
    signal(SIGILL,SIG_DFL);
# endif
#endif
    /*
     * Try printing out the errors, if there are none, or it was
     * attempted before, this call will just return.
     */
    print_errors();
    return 0;
}


/*
**  o p e n _ f e _ f i l e s
**
**  Opens all output files that are processed by the frontend, and returns the
**  file handles.  Note that if an optional file was not selected by the user,
**  the corresponding file handle will be NULL.
**
**  Returns:    FALSE if an error occurs opening any of the files.
*/

static boolean open_fe_files
#ifdef PROTO
(
    boolean     *cmd_opt,       /* [in] Array of command option flags */
    void        **cmd_val,      /* [in] Array of command option values */
    FILE        **lis_fid       /*[out] Listing file handle */
)
#else
(cmd_opt, cmd_val, lis_fid)
    boolean     *cmd_opt;       /* [in] Array of command option flags */
    void        **cmd_val;      /* [in] Array of command option values */
    FILE        **lis_fid;      /*[out] Listing file handle */
#endif

{
    /* Set up default return values. */
    *lis_fid = NULL;

    /* Create listing file if specified. */
    /* (add code when listing file supported) */

    return TRUE;
}

/*
**  o p e n _ b e _ f i l e s
**
**  Opens all output files that are processed by the backend, and returns the
**  file handles.  Note that if an optional file was not selected by the user,
**  the corresponding file handle will be NULL.
**
**  Implicit Inputs:    cmd_opt[opt_confirm] : If FALSE, normal processing.
**                      If TRUE, no files are actually created, but
**                      remaining processing remains intact.
**
**  Implicit Outputs:   cmd_opt flags for files are changed to reflect
**                      whether the files have been created or not.
**
**  Returns:    FALSE if an error occurs opening any of the files.
**              FALSE if no files were opened (none selected by user).
**              TRUE otherwise.
*/

static boolean open_be_files
#ifdef PROTO
(
    boolean     *cmd_opt,       /* [in] Array of command option flags */
    void        **cmd_val,      /* [in] Array of command option values */
    FILE        **h_fid,        /*[out] Header file handle */
    FILE        **caux_fid,     /*[out] Client auxiliary file handle */
    FILE        **saux_fid,     /*[out] Server auxiliary file handle */
    FILE        **cstub_fid,    /*[out] Client stub file handle */
    FILE        **sstub_fid,    /*[out] Server stub file handle */
    AST_interface_n_t *int_p    /* [in] Ptr to interface node */
)
#else
(cmd_opt, cmd_val, h_fid, caux_fid, saux_fid,
 cstub_fid, sstub_fid, int_p)
    boolean     *cmd_opt;       /* [in] Array of command option flags */
    void        **cmd_val;      /* [in] Array of command option values */
    FILE        **h_fid;        /*[out] Header file handle */
    FILE        **caux_fid;     /*[out] Client auxiliary file handle */
    FILE        **saux_fid;     /*[out] Server auxiliary file handle */
    FILE        **cstub_fid;    /*[out] Client stub file handle */
    FILE        **sstub_fid;    /*[out] Server stub file handle */
    AST_interface_n_t *int_p;   /* [in] Ptr to interface node */
#endif

{
    AST_export_n_t  *export_p;          /* Ptr to export node */
    boolean         stubs_required;     /* TRUE if stub generation required */
    boolean         status;             /* Modified by CALL macro */

    /* Set up default return values. */
    *caux_fid   = NULL;
    *saux_fid   = NULL;
    *h_fid      = NULL;
    *cstub_fid  = NULL;
    *sstub_fid  = NULL;

    /* Create header file if specified. */
    if (cmd_opt[opt_header])
    {
        if (cmd_opt[opt_verbose])
            message_print(NIDL_INCLCREATE, (char *)cmd_val[opt_header]);
        if (!cmd_opt[opt_confirm])
        {
            CALL(FILE_create((char *)cmd_val[opt_header], h_fid));
            fprintf(*h_fid, IDL_VERSION_TEMPLATE, IDL_VERSION_TEXT);
        }
    }

    /*
     * If there are no operations exported by the interface, or if the [local]
     * interface attribute is set, stub generation is not necessary.
     */
    stubs_required = FALSE;

    if (!AST_LOCAL_SET(int_p))
    {
        for (export_p = int_p->exports;
             export_p != NULL;
             export_p = export_p->next)
            if (export_p->kind == AST_operation_k)
            {
                stubs_required = TRUE;
                break;
            }
    }

    /* Create client stub file if necessary. */
    if (stubs_required
        &&  cmd_opt[opt_cstub]
        &&  cmd_opt[opt_emit_cstub])
    {
        if (cmd_opt[opt_verbose])
            message_print(NIDL_STUBCREATE, (char *)cmd_val[opt_cstub]);
        if (!cmd_opt[opt_confirm])
        {
            CALL(FILE_create((char *)cmd_val[opt_cstub], cstub_fid))
            fprintf(*cstub_fid, IDL_VERSION_TEMPLATE, IDL_VERSION_TEXT);
        }
    }
    else
        cmd_opt[opt_cstub] = FALSE;

    /* Create server stub file if necessary. */
    if (stubs_required
        &&  cmd_opt[opt_sstub]
        &&  cmd_opt[opt_emit_sstub])
    {
        if (cmd_opt[opt_verbose])
            message_print(NIDL_STUBCREATE, (char *)cmd_val[opt_sstub]);
        if (!cmd_opt[opt_confirm])
        {
            CALL(FILE_create((char *)cmd_val[opt_sstub], sstub_fid))
            fprintf(*sstub_fid, IDL_VERSION_TEMPLATE, IDL_VERSION_TEXT);
        }
    }
    else
        cmd_opt[opt_sstub] = FALSE;

    /* Create Client auxiliary file if it is necessary. */
    if (cmd_opt[opt_caux]
        &&  (int_p->sp_types != NULL
            ||  int_p->ool_types != NULL))
    {
        if (cmd_opt[opt_verbose])
            message_print(NIDL_STUBCREATE, (char *)cmd_val[opt_caux]);
        if (!cmd_opt[opt_confirm])
        {
            CALL(FILE_create((char *)cmd_val[opt_caux], caux_fid))
            fprintf(*caux_fid, IDL_VERSION_TEMPLATE, IDL_VERSION_TEXT);
        }
    }
    else
        cmd_opt[opt_caux] = FALSE;

    /* Create Server auxiliary file if it is necessary. */
    if (cmd_opt[opt_saux]
        &&  (int_p->sp_types != NULL
            ||  int_p->ool_types != NULL
            ||  int_p->pipe_types != NULL))
    {
        if (cmd_opt[opt_verbose])
            message_print(NIDL_STUBCREATE, (char *)cmd_val[opt_saux]);
        if (!cmd_opt[opt_confirm])
        {
            CALL(FILE_create((char *)cmd_val[opt_saux], saux_fid))
            fprintf(*saux_fid, IDL_VERSION_TEMPLATE, IDL_VERSION_TEXT);
        }
    }
    else
        cmd_opt[opt_saux] = FALSE;

    return (*caux_fid   != NULL
            ||  *saux_fid   != NULL
            ||  *h_fid      != NULL
            ||  *cstub_fid  != NULL
            ||  *sstub_fid  != NULL);
}

/*
**  s t u b _ c o m p i l e
**
**  Conditionally invokes the C compiler to compile a generated stub file,
**  directing the output file to the same directory as the source file.
**  Conditionally outputs message that stub is being compiled.
*/

static int stub_compile
#ifdef PROTO
(
    boolean     *cmd_opt,       /* [in] Array of command option flags */
    void        **cmd_val,      /* [in] Array of command option values */
    int         opt_file,       /* [in] Index of stub file to process */
    FILE        *fid,           /* [in] File handle of stub file */
    char        *compile_cmd    /* [in] Base command to compile stub */
)
#else
(cmd_opt, cmd_val, opt_file, fid, compile_cmd)
    boolean     *cmd_opt;       /* [in] Array of command option flags */
    void        **cmd_val;      /* [in] Array of command option values */
    int         opt_file;       /* [in] Index of stub file to process */
    FILE        *fid;           /* [in] File handle of stub file */
    char        *compile_cmd;   /* [in] Base command to compile stub */
#endif

{
    char    compile_opt[max_string_len];
    char    filespec[PATH_MAX];

    /* If file was disabled, just return. */
    if (!cmd_opt[opt_file])
        return pgm_ok;

    /* Conditionally output "Compiling stub file" message. */
    if (cmd_opt[opt_verbose] && error_count == 0 && cmd_opt[opt_confirm])
        message_print(NIDL_STUBCOMPILE, (char *)cmd_val[opt_file]);

    /* If this is just a -confirm pass then return now. */
    if (cmd_opt[opt_confirm] || fid == NULL)
        return pgm_ok;

    compile_opt[0] = '\0';

    FILE_parse((char *)cmd_val[opt_file], filespec, (char *)NULL, (char *)NULL);
    if (!FILE_is_cwd(filespec))
#ifdef CC_OPT_OBJECT
        /*
         * Make sure the object file ends up in the same directory as source
         * file by forming the object file name from the source file name and
         * constructing a command option to the C compiler.  (This can't be
         * done, however, if the user has specified a -cc_cmd other than the
         * default, since we can't be sure of valid C compiler options.)
         */
        if (strcmp((char *)cmd_val[opt_cc_cmd], CC_DEF_CMD) == 0)
        {
            FILE_form_filespec((char *)NULL, (char *)NULL, OBJ_FILETYPE,
                               (char *)cmd_val[opt_file], filespec);

            sprintf(compile_opt, "%s%s", CC_OPT_OBJECT, filespec);
        }
        else
#endif
            message_print(NIDL_OUTDIRIGN, (char *)cmd_val[opt_file]);

    return FILE_execute_cmd(compile_cmd, compile_opt, (char *)cmd_val[opt_file],
        ((cmd_opt[opt_verbose]) ? NIDL_STUBCOMPILE: 0)
        );
}

/*
**  c l o s e _ f i l e s
**
**  Closes any output files that were opened during the compilation.
*/

static void close_files
#ifdef PROTO
(
    FILE        *lis_fid,       /* [in] Listing file handle */
    FILE        *h_fid,         /* [in] Header file handle */
    FILE        *caux_fid,      /* [in] Client auxiliary file handle */
    FILE        *saux_fid,      /* [in] Server auxiliary file handle */
    FILE        *cstub_fid,     /* [in] Client stub file handle */
    FILE        *sstub_fid      /* [in] Server stub file handle */
)
#else
(lis_fid, h_fid, caux_fid, saux_fid, cstub_fid, sstub_fid)
    FILE        *lis_fid;       /* [in] Listing file handle */
    FILE        *h_fid;         /* [in] Header file handle */
    FILE        *caux_fid;      /* [in] Client auxiliary file handle */
    FILE        *saux_fid;      /* [in] Server auxiliary file handle */
    FILE        *cstub_fid;     /* [in] Client stub file handle */
    FILE        *sstub_fid;     /* [in] Server stub file handle */
#endif

{
    if (lis_fid     != NULL) fclose(lis_fid);
    if (h_fid       != NULL) fclose(h_fid);
    if (caux_fid    != NULL) fclose(caux_fid);
    if (saux_fid    != NULL) fclose(saux_fid);
    if (cstub_fid   != NULL) fclose(cstub_fid);
    if (sstub_fid   != NULL) fclose(sstub_fid);
}

/*
**  i n i t
**
**  Does any initializations that are global to the entire compiler.
**  Component-specific initialization should be done by a separate
**  initialization function for that component.
*/

#ifdef PROTO
static boolean init(char *image_name)       /* Returns TRUE on success */
#else
static boolean init(image_name)           /* Returns TRUE on success */
      char *image_name;
#endif

{
    /* Open error message database. */
    message_open(image_name);

    return TRUE;
}


/*
**  c l e a n u p
**
**  Does any cleanup that is global to the entire compiler.
**  Component-specific cleanup should be done by a separate
**  cleanup function for that component.
*/

#ifdef PROTO
static boolean cleanup(void)    /* Returns TRUE on success */
#else
static boolean cleanup()        /* Returns TRUE on success */
#endif

{
    /* Close error message database. */
    message_close();

    return TRUE;
}



/*
**  D R I V E R _ m a i n
**
**  Invokes each major compiler component.
*/

boolean DRIVER_main
#ifdef PROTO
(
    int         argc,           /* Command line argument count */
    char        **argv          /* Array of command line arguments */
)
#else
(argc, argv)
    int         argc;           /* Command line argument count */
    char        **argv;         /* Array of command line arguments */
#endif

{
    boolean     *cmd_opt;       /* Array of command option flags */
    void        **cmd_val;      /* Array of command option values */
    STRTAB_str_t idl_sid;       /* IDL filespec stringtable ID */
    FILE        *lis_fid;       /* Listing file handle */
    FILE        *h_fid;         /* Header file handle */
    FILE        *caux_fid;      /* Client auxiliary file handle */
    FILE        *saux_fid;      /* Server auxiliary file handle */
    FILE        *cstub_fid;     /* Client stub file handle */
    FILE        *sstub_fid;     /* Server stub file handle */
    AST_interface_n_t *int_p;   /* Ptr to interface node */
    boolean     status = TRUE;  /* Assume we are successful */
    int         tmpsts;

    /*
     * Null out all file pointers in case we get an error we know which
     * have been opened.
     */
    lis_fid = NULL;
    h_fid = NULL;
    caux_fid = NULL;
    saux_fid = NULL;
    cstub_fid = NULL;
    sstub_fid = NULL;

    cmd_opt = NULL;
    cmd_val = NULL;


    /*
     * Establish a handler such that we always try to output the
     * error messages, even when the compiler has an internal error.
     */
#ifdef vms
    VAXC$ESTABLISH(attempt_to_print_errors);
#else
# if defined(SNI_SVR4_POSIX)
	{
	struct sigaction act;

	act.sa_handler = (void (*)()) attempt_to_print_errors;
	(void) sigemptyset( &act.sa_mask );

	/*
	*  By setting the flag SA_RESTHAND the signal disposition is
	*	reset to SIG_DFL and the signal will not be blocked
	*	on entry to the signal handler.
	*/
	act.sa_flags = SA_RESETHAND;

	(void) sigaction(SIGBUS, &act, (struct sigaction *)NULL);
	(void) sigaction(SIGSEGV, &act, (struct sigaction *)NULL);
	(void) sigaction(SIGFPE, &act, (struct sigaction *)NULL);
	(void) sigaction(SIGILL, &act, (struct sigaction *)NULL);
	}
# else
    signal(SIGBUS, (void (*)())attempt_to_print_errors);
    signal(SIGSEGV, (void (*)())attempt_to_print_errors);
    signal(SIGFPE, (void (*)())attempt_to_print_errors);
    signal(SIGILL, (void (*)())attempt_to_print_errors);
# endif
#endif


    /*
     * This point is established for orderly termination of the compilation.
     * If a fatal error is detected, execution continues following this
     * if statement and the normal cleanup is performed.
     */
#if defined(SNI_SVR4_POSIX)
    if (sigsetjmp(nidl_termination_jmp_buf, 1) == 0)
#else
    if (setjmp(nidl_termination_jmp_buf) == 0)
#endif
    {
        /* Global initialization (not specific to any one component). */
        CALL(init(argv[0]));

        /*
         * Command line parsing.  **NOTE**: If the combination of options
         * -confirm -v is selected, control *will* return back here; thus
         * make sure not to call any component if cmd_opt[opt_confirm] is true.
         */
        CALL(CMD_parse_args(argc-1, &argv[1], &cmd_opt, &cmd_val, &idl_sid));

        /*
         * Open output files that are processed by the frontend.  Returned file
         * handles are null if not applicable.  The source IDL files, any imported
         * IDL files, and any ACF files are opened by the parse routines.
         */
        if (!cmd_opt[opt_confirm])
            CALL(open_fe_files(cmd_opt, cmd_val, &lis_fid));

        /* Frontend processing. */
        BE_push_malloc_ctx();
        CALL(FE_main(cmd_opt, cmd_val, idl_sid, &int_p));
        BE_pop_malloc_ctx();

        /* Print accumulated errors and warnings generated by frontend. */
        if (cmd_opt == NULL || !cmd_opt[opt_confirm])
            print_errors();

        /* Backend processing. */
        if (open_be_files(cmd_opt, cmd_val, &h_fid, &caux_fid,
                &saux_fid, &cstub_fid, &sstub_fid, int_p))
        {
            char    filename[PATH_MAX];
            char    filetype[PATH_MAX];
            char    *saved_header;
            int     len;

            /* Prune the header filespec for BE so only the leafname remains. */
            FILE_parse((char *)cmd_val[opt_header], NULL, filename, filetype);
            strcat(filename, filetype);
            saved_header = (char *)cmd_val[opt_header];
            cmd_val[opt_header] = (void *)filename;

            CALL(BE_main(cmd_opt, cmd_val, h_fid, caux_fid, saux_fid,
                         cstub_fid, sstub_fid, int_p));

            cmd_val[opt_header] = (void *)saved_header;
        }
    }
    else status = FALSE;

    /* Generation of listing file. */
    /* (add call here) */

    /* Statistics reporting. */
    /* (add call here) */

    /* Print accumulated errors and warnings generated by the backend, if any. */
    if (cmd_opt == NULL || !cmd_opt[opt_confirm])
        print_errors();

    /* Close all output files. */
    close_files(lis_fid, h_fid, caux_fid, saux_fid,
                cstub_fid, sstub_fid);

    /* Compile any generated files if objects were requested */
    if (cmd_opt != NULL && cmd_opt[opt_keep_obj] && status)
    {
        char **idir_list;               /* Array of include directories */
        char idir_opt[max_string_len];  /* Cmd list of include directories */
        char cc_cmd[max_string_len];    /* Base command line and options */
#ifdef VMS
        boolean     paren_flag;         /* TRUE if trailing paren needed */
#endif

        /* Paste together command line option for include directories. */

#if defined(UNIX) || defined(VMS)
        idir_list = (char **)cmd_val[opt_idir];
        idir_opt[0] = '\0';

#ifdef VMS
        if (*idir_list || cmd_opt[opt_out])
        {
            paren_flag = TRUE;
            strcat(idir_opt, " /INCLUDE_DIRECTORY=(");
        }
        else
            paren_flag = FALSE;
#endif

        /* If an -out directory was specified, place it at front of idir list */
        if (cmd_opt[opt_out])
        {
#ifdef UNIX
            strcat(idir_opt, " -I");
#endif
            strcat(idir_opt, (char *)cmd_val[opt_out]);
#ifdef VMS
            strcat(idir_opt, ",");
#endif
        }

        while (*idir_list)
        {
#ifdef UNIX
            strcat(idir_opt, " -I");
#endif
            /*
             * If this include dir is the system IDL dir, then replace it
             * with the system H dir, which might be different.
             */
            if (strcmp(*idir_list, DEFAULT_IDIR) == 0)
                strcat(idir_opt, DEFAULT_H_IDIR);
            else
                strcat(idir_opt, *idir_list);
            idir_list++;
#ifdef VMS
            strcat(idir_opt, ",");
#endif
        }

#ifdef VMS
        if (paren_flag)
            /* Overwrite trailing comma with paren. */
            idir_opt[strlen(idir_opt)-1] = ')';
#endif
#endif

        /* Paste together base command line. */

#ifdef PASS_I_DIRS_TO_CC
        sprintf(cc_cmd, "%s %s %s", (char *)cmd_val[opt_cc_cmd],
                (char *)cmd_val[opt_cc_opt], idir_opt);
#else
        sprintf(cc_cmd, "%s %s", (char *)cmd_val[opt_cc_cmd],
                (char *)cmd_val[opt_cc_opt]);
#endif

        /* Now compile the stub modules. */

        tmpsts = stub_compile(cmd_opt, cmd_val, opt_cstub, cstub_fid, cc_cmd);
        if (ERROR_STATUS(tmpsts)) status = FALSE;
	else
	    tmpsts = stub_compile(cmd_opt, cmd_val, opt_sstub, sstub_fid, cc_cmd);
        if (ERROR_STATUS(tmpsts)) status = FALSE;
	else
	    tmpsts = stub_compile(cmd_opt, cmd_val, opt_caux, caux_fid, cc_cmd);
        if (ERROR_STATUS(tmpsts)) status = FALSE;
	else
	    tmpsts = stub_compile(cmd_opt, cmd_val, opt_saux, saux_fid, cc_cmd);
        if (ERROR_STATUS(tmpsts)) status = FALSE;
    }

    /* If no request to keep C files, delete each of them. */
    if (cmd_opt != NULL && !cmd_opt[opt_keep_c] && status)
    {
        if (cmd_opt[opt_cstub])
        {
            if (cmd_opt[opt_verbose] && error_count == 0)
                message_print(NIDL_STUBDELETE, (char *)cmd_val[opt_cstub]);
            if (!cmd_opt[opt_confirm] && cstub_fid)
                FILE_delete((char *)cmd_val[opt_cstub]);
        }

        if (cmd_opt[opt_sstub])
        {
            if (cmd_opt[opt_verbose] && error_count == 0)
                message_print(NIDL_STUBDELETE, (char *)cmd_val[opt_sstub]);
            if (!cmd_opt[opt_confirm] && sstub_fid)
                FILE_delete((char *)cmd_val[opt_sstub]);
        }

        if (cmd_opt[opt_caux])
        {
            if (cmd_opt[opt_verbose] && error_count == 0)
                message_print(NIDL_STUBDELETE, (char *)cmd_val[opt_caux]);
            if (!cmd_opt[opt_confirm] && caux_fid)
                FILE_delete((char *)cmd_val[opt_caux]);
        }

        if (cmd_opt[opt_saux])
        {
            if (cmd_opt[opt_verbose] && error_count == 0)
                message_print(NIDL_STUBDELETE, (char *)cmd_val[opt_saux]);
            if (!cmd_opt[opt_confirm] && saux_fid)
                FILE_delete((char *)cmd_val[opt_saux]);
        }
    }

    /* Global cleanup. */
    if (!cleanup()) status = FALSE;

    return status;
}

