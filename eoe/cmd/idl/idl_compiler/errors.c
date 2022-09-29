/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: errors.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:39:16  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:34:36  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:46:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:01:51  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/04  03:19:00  harrow
 * 	Add missing initialization of text_len reported by defect 2118
 * 	[1992/03/04  03:18:16  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:06  devrcs
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
**  NAME
**
**      ERRORS.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      IDL Error logging and reporting routines.
**
**  VERSION: DCE 1.0
**
*/


#include <nidl.h>
#include <errors.h>
#include <nametbl.h>
#include <frontend.h>
#include <message.h>
#include <nidlmsg.h>
#include <driver.h>

#define MAX_LINE_LEN    256
#define WHERE_TEXT_LEN   20
#define MAX_WARNINGS    5
#define MAX_ERROR_FILES 10


/*
 *  Error log record format.
 */
typedef struct error_log_rec_t
{
    STRTAB_str_t filename;      /* Error file name */
    int     lineno;            /* Source line number */
    long    msg_id;             /* Message identifer */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;  /* Error Arguments */
    union                       /* Tree node or List node */
    {
        struct
        {                                   /* As tree node: */
            struct  error_log_rec_t *left;  /* Left pointer */
            struct  error_log_rec_t *right; /* Right pointer */
        } asBinTree;
        struct
        {                                   /* As list node: */
            struct  error_log_rec_t *next;  /* Ptr to next for this lineno */
        } asList;
    } links;
    struct  error_log_rec_t *first_this_line;   /* 1st addtl msg this line */
    struct  error_log_rec_t *last_this_line;    /* last addtl msg this line */
} error_log_rec_t;



/* Pointers to IDL parser or ACF parser global variables. */

FILE    **yyin_p;           /* Points to yyin or acf_yyin */
int     *yylineno_p;        /* Points to yylineno or acf_yylineno */
int     *yynerrs_p;         /* Points to yynerrs or acf_yynerrs */
char    *yytext_p;          /* Points to yytext or acf_yytext */

boolean ERR_no_warnings;    /* Global copy of -no_warn command line option */

static  int              warnings = 0;      /* Warning count */
static  STRTAB_str_t    error_files[MAX_ERROR_FILES]; /* List of all files with errors */
static  int     error_file_count = 0;       /* Number of files with errors */
static  int     last_error_line = 0;        /* Line of last error */
static  char    *current_file   = NULL;     /* Current source file name */
        error_log_rec_t  *errors = NULL;    /* Tree root for error nodes */
        int     error_count     = 0;        /* Error count */
        STRTAB_str_t    error_file_name_id; /* Id of current source file */

extern void sysdep_cleanup_temp (
#ifdef PROTO
    void
#endif
);

/*
 *  y y w h e r e
 *
 *  Function:   Returns current input position for yyparse.
 *
 *  Globals:    yy*_p
 *
 *  Notes:      This was adapted from the book
 *                  "Compiler Construction under Unix"
 *              by A. T. Schreiner & H. G. Friedman
 *
 *              Rico 16-Oct-89: Considerably changed original algorithm.
 */

void yywhere
#ifdef PROTO
(
    void
)
#else
()
#endif

{
    boolean have_text = false;  /* True if have source text to output */
    int     text_len = 0;       /* Length of source text to output */
    int     lineno;             /* Source line number of relevant text */
    long    msg_id;             /* ID of message to output */
    char    *near_text;         /* Text of object near error */
    STRTAB_str_t string_id;     /* Entry in string table of near text */

    lineno = *yylineno_p;

    if (*yytext_p)
    {
        for (text_len = 0; text_len < WHERE_TEXT_LEN; ++text_len)
            if (!yytext_p[text_len] || yytext_p[text_len] == '\n')
                break;

        if (text_len > 0)
        {
            have_text = true;
            lineno = lineno - (*yytext_p == '\n' || ! *yytext_p);
        }
    }

    /*
     * If there is some text to show, put it in the string table
     */
    if (have_text)
    {
        string_id = STRTAB_add_string(yytext_p);
        STRTAB_str_to_string(string_id, &near_text);
    }

    if (have_text)
    {
        if (feof(*yyin_p))
            msg_id = NIDL_EOFNEAR;
        else
            msg_id = NIDL_SYNTAXNEAR;
    }
    else
    {
        if (feof(*yyin_p))
            msg_id = NIDL_EOF;
        else
            msg_id = NIDL_SYNTAXNEAR;
    }

    log_error(lineno, msg_id, text_len, near_text);
}

/*
 *  y y e r r o r
 *
 *  Function:   Called by yypaser when a parse error is encountered.
 *
 *  Inputs:     message -  error message to display
 *              token    - expected token
 *
 *  Globals:    yynerrs_p
 *
 *  Notes:      This was adapted from the book
 *                  "Compiler Construction under Unix"
 *              by A. T. Schreiner & H. G. Friedman
 */

void yyerror
#ifdef PROTO
(
    char *message
)
#else
(message)
    char *message;
#endif

{
    static int list = 0;        /* Note: Currently always 0 */

    if (message || !list)
    {
        (*yynerrs_p)++;
        yywhere();

        /*
         * Only print the yacc version of errors if it isn't for a syntax error.
         */
        if (strcmp(message,"syntax error") != 0)
        {
            fputs(message, stderr);
            putc('\n', stderr);
            return;
        }

        return;
    }

    putc('\n', stderr);
    list = 0;
}

/*
 *  a l l o c _ l o g _ r e c
 *
 *  Function:   Allocates and initializes error log record.
 *
 *  Inputs:     lineno - source line number
 *              message - message text
 *
 *  Outputs:    Address of error log record.
 */

static error_log_rec_t *alloc_log_rec
#ifdef PROTO
(
    STRTAB_str_t filename,
    int  lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(filename, lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    STRTAB_str_t filename;
    int             lineno;
    long            msg_id;
    char            *arg1;
    char            *arg2;
    char            *arg3;
    char            *arg4;
    char            *arg5;
#endif

{
    error_log_rec_t *log_rec_p;
    int             i;


    log_rec_p = (error_log_rec_t *) MALLOC(sizeof(error_log_rec_t));

    log_rec_p->lineno  = lineno;
    log_rec_p->filename = filename;
    log_rec_p->msg_id = msg_id;
    log_rec_p->arg1 = arg1;
    log_rec_p->arg2 = arg2;
    log_rec_p->arg3 = arg3;
    log_rec_p->arg4 = arg4;
    log_rec_p->arg5 = arg5;

    log_rec_p->first_this_line = 0;
    log_rec_p->last_this_line  = 0;

    log_rec_p->links.asBinTree.left  = 0;
    log_rec_p->links.asBinTree.right = 0;

    log_rec_p->links.asList.next     = 0;

    /*
     * If this file is not listed in the error file list
     */
    for (i = 0; i < error_file_count; i++)
        if (error_files[i] == filename) break;

    /*
     * Add the file with errors to the error_file list
     */
    if (i == error_file_count)
        error_files[error_file_count++] = filename;

    /*
     * If we have too many files with errors, just ignore some
     */
    if (error_file_count >= MAX_ERROR_FILES) error_file_count--;

    return  log_rec_p;
}

/*
 *  f r e e _ l o g _ r e c
 *
 *  Function:   Frees an error log record and anything below it
 *
 *  Inputs:     record - error log record to free
 *  Outputs:    none
 */

static void free_log_rec
#ifdef PROTO
(
    error_log_rec_t *log_rec_p
)
#else
(log_rec_p)
    error_log_rec_t *log_rec_p;
#endif

{
    if (log_rec_p->links.asBinTree.left  != NULL)
    {
        free_log_rec(log_rec_p->links.asBinTree.left);
        log_rec_p->links.asBinTree.left = NULL;
    }

    if (log_rec_p->links.asBinTree.right  != NULL)
    {
        free_log_rec(log_rec_p->links.asBinTree.right);
        log_rec_p->links.asBinTree.right = NULL;
    }


    /*
     * Free all errors logged for this line number
     */
    while (log_rec_p->first_this_line != NULL)
    {
        error_log_rec_t *ep;
        ep = log_rec_p->first_this_line;
        log_rec_p->first_this_line = log_rec_p->links.asList.next;
        FREE(ep);
    }

    FREE(log_rec_p);
}

/*
 *  q u e u e _ e r r o r
 *
 *  Function:   Adds an error message to the end of a list of
 *              errors queued for a particular line.
 *
 *  Inputs:     log_rec_p - a pointer to the header error log rec.
 *              message   - the error message.
 */

static void queue_error
#ifdef PROTO
(
    error_log_rec_t *log_rec_p,
    STRTAB_str_t filename,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(log_rec_p, filename, msg_id, arg1, arg2, arg3, arg4, arg5)
    error_log_rec_t *log_rec_p;
    STRTAB_str_t filename;
    long            msg_id;
    char            *arg1;
    char            *arg2;
    char            *arg3;
    char            *arg4;
    char            *arg5;
#endif

{
    error_log_rec_t *new_log_rec_p;

    new_log_rec_p = alloc_log_rec(filename, log_rec_p->lineno, msg_id, arg1, arg2, arg3, arg4, arg5);

    if (log_rec_p->first_this_line == NULL)
    {
        log_rec_p->first_this_line = (struct error_log_rec_t *) new_log_rec_p;
        log_rec_p->last_this_line  = (struct error_log_rec_t *) new_log_rec_p;
        return;
    }

    log_rec_p->last_this_line->links.asList.next =
        (struct error_log_rec_t *) new_log_rec_p;
    log_rec_p->last_this_line =
        (struct error_log_rec_t *) new_log_rec_p;
}

/*
 *  a d d _ e r r o r _ l o g _ r e c
 *
 *  Function:   Adds an error log to the sorted binary tree of
 *              error messages.
 *
 *  Inputs:     log_rec_p - pointer to current root of tree.
 *              lineno    - line number on which error occurred.
 *              message   - the error message.
 */

static void add_error_log_rec
#ifdef PROTO
(
    error_log_rec_t *log_rec_p,
    STRTAB_str_t filename,
    int lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(log_rec_p, filename, lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    error_log_rec_t *log_rec_p;
    STRTAB_str_t filename;
    int             lineno;
    long            msg_id;
    char            *arg1;
    char            *arg2;
    char            *arg3;
    char            *arg4;
    char            *arg5;
#endif

{
    if (log_rec_p->lineno < lineno)
    {
        if (log_rec_p->links.asBinTree.right != NULL)
            add_error_log_rec(log_rec_p->links.asBinTree.right, filename, lineno,
                    msg_id, arg1, arg2, arg3, arg4, arg5);
        else
            log_rec_p->links.asBinTree.right = alloc_log_rec(filename, lineno,
                    msg_id, arg1, arg2, arg3, arg4, arg5);

        return;
    }

    if (log_rec_p->lineno > lineno)
    {
        if (log_rec_p->links.asBinTree.left != NULL)
            add_error_log_rec(log_rec_p->links.asBinTree.left, filename, lineno,
                    msg_id, arg1, arg2, arg3, arg4, arg5);

        else
            log_rec_p->links.asBinTree.left = alloc_log_rec(filename, lineno,
                    msg_id, arg1, arg2, arg3, arg4, arg5);
        return;
    }

    if (log_rec_p->lineno == lineno)

queue_error(log_rec_p, filename, msg_id, arg1, arg2, arg3, arg4, arg5);
}

/*
 *  l o g _ s o u r c e _ e r r o r
 *
 *  Function:   Accumulates an error message for later printout.
 *              All accumulated errors are printed by print_errors.
 *              Errors are kept sorted by line number and source
 *              file name.
 *
 *  Inputs:
 *              filename - STRTAB_str_t of full source file name
 *              lineno  - the line number of the error.
 *              msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Outputs:    An error log record is inserted in the error tree.
 *
 */

void log_source_error
#ifdef PROTO
(
    STRTAB_str_t filename,
    int lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(filename, lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    STRTAB_str_t filename;      /* file name string */
    int     lineno;             /* Source line number */
    long    msg_id;             /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    ++error_count;

    if (errors == NULL)
        errors = alloc_log_rec(filename, lineno, msg_id, (char *)arg1, (char *)arg2, (char *)arg3, (char *)arg4, (char *)arg5);
    else
        add_error_log_rec(errors, filename, lineno, msg_id, (char *)arg1, (char *)arg2, (char *)arg3, (char *)arg4, (char *)arg5);
}

/*
 *  l o g _ s o u r c e _ w a r n i n g
 *
 *  Function:   Accumulates a warning message for later printout.
 *              All accumulated errors are printed by print_errors.
 *              Errors are kept sorted by line number and source
 *              file name.
 *
 *  Inputs:
 *              filename - STRTAB_str_t of full source file name
 *              lineno  - the line number of the error.
 *              msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Outputs:    An error log record is inserted in the error tree.
 *
 */

void log_source_warning
#ifdef PROTO
(
    STRTAB_str_t filename,
    int lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(filename, lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    STRTAB_str_t filename; /* source file name */
    int     lineno;     /* Source line number */
    long    msg_id;      /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    /* Return if warnings are suppressed. */
    if (ERR_no_warnings)
        return;

    ++warnings;

    if (errors == NULL)
        errors = alloc_log_rec(filename, lineno, msg_id, (char *)arg1, (char *)arg2, (char *)arg3, (char *)arg4, (char *)arg5);
    else
        add_error_log_rec(errors, filename, lineno, msg_id, (char *)arg1, (char *)arg2, (char *)arg3, (char *)arg4, (char *)arg5);
}

/*
 *  l o g _ e r r o r
 *
 *  Function:   Accumulates an error message for later printout.
 *              All accumulated errors are printed by print_errors.
 *              Errors are kept sorted by line number.  The error
 *              is logged on the file for which set_name_for_errors
 *              was most recently called.
 *
 *  Inputs:     lineno  - the line number of the error.
 *              msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Outputs:    An error log record is inserted in the error tree.
 *
 */

void log_error
#ifdef PROTO
(
    int lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    int     lineno;     /* Source line number */
    long    msg_id;      /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    log_source_error (error_file_name_id, lineno, msg_id, arg1, arg2, arg3, arg4, arg5);
}

/*
 *  l o g _ w a r n i n g
 *
 *  Function:   Accumulates a warning message for later printout.
 *              All accumulated errors are printed by print_errors.
 *              Errors are kept sorted by line number.  The warning
 *              is logged on the file for which set_name_for_errors
 *              was most recently called.
 *
 *  Inputs:     lineno  - the line number of the warning.
 *              msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Outputs:    An error log record is inserted in the error tree.
 *
 */

void log_warning
#ifdef PROTO
(
    int lineno,
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(lineno, msg_id, arg1, arg2, arg3, arg4, arg5)
    int     lineno;     /* Source line number */
    long    msg_id;      /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    log_source_warning (error_file_name_id, lineno, msg_id, arg1, arg2, arg3, arg4, arg5);
}


/*
 *  s e e k _ f o r _ l i n e
 *
 *  Function:   Reads the line specified by lineno.
 *
 *  Inputs:     source - the file descriptor for the source file.
 *              lineno - the number of the line in error.
 *
 *  Outputs:    source_line - the source line is returned through here
 *
 *  Globals:    last_error_line
 */

void seek_for_line
#ifdef PROTO
(
    FILE *source_file,
    int lineno,
    char *source_line
)
#else
(source_file, lineno, source_line)
    FILE    *source_file;
    int     lineno;
    char    *source_line;
#endif

{
    int lines_to_skip;
    int i;

    /*
     * If the FILE is NULL then can't get the source
     */
    if (source_file == NULL)
    {
        source_line[0] = 0;
        return;
    }

    lines_to_skip = lineno - last_error_line;

    for (i=0; i<lines_to_skip; i++)
        (void) fgets(source_line, MAX_LINE_LEN, source_file);

    /* Strip off newline. */
    i = strlen(source_line) - 1;
    if (source_line[i] == '\n')
        source_line[i] = '\0';

    last_error_line = lineno;
}

/*
 *  p r i n t _ e r r o r s _ f o r _ l i n e
 *
 *  Function:   Prints out a source line and accumulated errors
 *              for that line.
 *
 *  Inputs:     fd - file descriptor for source file
 *              log_rec_ptr - a pointer to the header log rec for the line
 *              source - source file name
 */

void print_errors_for_line
#ifdef PROTO
(
    FILE *fd,
    char *source,
    STRTAB_str_t    source_id,
    error_log_rec_t *log_rec_ptr
)
#else
(fd, source, source_id, log_rec_ptr)
    FILE            *fd;
    char            *source;
    STRTAB_str_t    source_id;
    error_log_rec_t *log_rec_ptr;
#endif

{
    char            source_line[MAX_LINE_LEN];
    boolean         source_printed = false;
    error_log_rec_t *erp;


    /* Print the error only if it in this file */
    if (source_id == log_rec_ptr->filename)
    {
        source_printed = true;
        seek_for_line(fd, log_rec_ptr->lineno, source_line);
        message_print(
            NIDL_FILESOURCE, source, log_rec_ptr->lineno, source_line
        );
        message_print(
            log_rec_ptr->msg_id, log_rec_ptr->arg1, log_rec_ptr->arg2,
            log_rec_ptr->arg3, log_rec_ptr->arg4, log_rec_ptr->arg5
        );
    }


    for (erp = log_rec_ptr->first_this_line; erp; erp=erp->links.asList.next)
    {
        /* Print the error only if it in this file */
        if (source_id == erp->filename)
        {
            /* If we haven't output the source line text yet, the do so */
            if (!source_printed)
            {
                source_printed = true;
                seek_for_line(fd, log_rec_ptr->lineno, source_line);
                message_print(
                    NIDL_FILESOURCE, source, log_rec_ptr->lineno, source_line
                );
            }

            /* Now print out the actual error message */
            message_print(
                erp->msg_id, erp->arg1, erp->arg2, erp->arg3, erp->arg4, erp->arg5
            );
        }
    }
}

/*
 *  p r i n t _ e r r o r _ m e s s a g e s
 *
 *  Function:   Recursively prints all accumulated error messages.
 *
 *  Inputs:     fd          - file descriptor for source file
 *              source      - name of source file
 *              log_rec_ptr - root of error log tree
 */

void print_error_messages
#ifdef PROTO
(
    FILE *fd,
    char *source,
    STRTAB_str_t    source_id,
    error_log_rec_t *log_rec_ptr
)
#else
(fd, source, source_id, log_rec_ptr)
    FILE            *fd;
    char            *source;
    STRTAB_str_t    source_id;
    error_log_rec_t *log_rec_ptr;
#endif

{
    if (log_rec_ptr->links.asBinTree.left != NULL)
        print_error_messages(fd, source, source_id, log_rec_ptr->links.asBinTree.left);

    print_errors_for_line(fd, source, source_id, log_rec_ptr);

    if (log_rec_ptr->links.asBinTree.right != NULL)
        print_error_messages(fd, source, source_id, log_rec_ptr->links.asBinTree.right);
}

/*
 *  p r i n t _ e r r o r s
 *
 *  Function:   Prints all accumulated error messages.
 *
 *  Inputs:     source - String table ID of source file name
 *
 *  Functional value:   true - if any errors were printed
 *                      false otherwise
 *
 */

boolean print_errors
(
#ifdef PROTO
    void
#endif
)

{
    FILE    *fd;
    char    *fn;
    int     i;
    STRTAB_str_t   source_id;
    error_log_rec_t *error_root;

    /*
     * If there are no errors, just return
     */
     if (errors == NULL) return 0;


    /*
     * Copy the root of the error tree and null it out so that
     * it looks like all the errors have been printed if we get
     * an error while printing them out.
     */
     error_root = errors;
     errors = NULL;

    /*
     * Loop through all files with errors
     */
    for (i = 0; i < error_file_count; i++)
    {
        source_id = error_files[i];
        STRTAB_str_to_string(source_id, &fn);
        fd = fopen(fn, "r");
        print_error_messages(fd, fn, source_id, error_root);
        last_error_line = 0;
    }

    /*
     * Free the tree of errors,
     */
    free_log_rec(error_root);
    error_file_count = 0;

    /*
     * Return true if we found any errors.
     */
    return (error_file_count != 0);
}

/*
 *  e r r o r
 *
 *  Function:   Prints the specifed error message and terminates the program
 *
 *  Inputs:     msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Notes:      This call terminates the calling program with a failure status
 *
 */

void error
#ifdef PROTO
(
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(msg_id, arg1, arg2, arg3, arg4, arg5)
    long    msg_id;      /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    if (current_file)
        message_print(NIDL_LINEFILE, current_file, *yylineno_p);
    message_print(msg_id, arg1, arg2, arg3, arg4, arg5);

#ifndef HASPOPEN
    sysdep_cleanup_temp();
#endif

    nidl_terminate();
}

/*
 *  e r r o r _ l i s t
 *
 *  Function:   Prints the specifed error message(s) and terminates the program
 *
 *  Inputs:     vecsize - number of messages
 *              errvec - pointer to one or more message info elements
 *              exitflag - TRUE => exit program
 *
 *  Notes:      This call terminates the calling program with a failure status
 *
 */

void error_list
#ifdef PROTO
(
    int vecsize,
    idl_error_list_p errvec,
    boolean exitflag
)
#else
(vecsize, errvec, exitflag)
    int vecsize;
    idl_error_list_p errvec;
    boolean exitflag;
#endif

{
    int i;

    if (current_file)
        message_print(NIDL_LINEFILE, current_file, *yylineno_p);

    for (i = 0; i < vecsize; i++)
        message_print(errvec[i].msg_id, errvec[i].arg1, errvec[i].arg2,
                      errvec[i].arg3, errvec[i].arg4, errvec[i].arg5);

    if (!exitflag) return;

#ifndef HASPOPEN
    sysdep_cleanup_temp();
#endif

    nidl_terminate();
}

/*
 *  w a r n i n g
 *
 *  Function:   Prints the specifed error message. Terminates if the
 *              error count excees the threshold.
 *
 *  Inputs:     msg_id - the error message ID.
 *              [arg1,...,arg5] - 0 to 5 arguments for error message formatting
 *
 *  Globals:    yylineno_p
 *
 *  Notes:      This call terminates the calling program with a status of -2.
 *
 */

void warning
#ifdef PROTO
(
    long msg_id,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
#else
(msg_id, arg1, arg2, arg3, arg4, arg5)
    long    msg_id;      /* Message ID */
    char    *arg1, *arg2, *arg3, *arg4, *arg5;
#endif

{
    /* Return if warnings are suppressed. */
    if (ERR_no_warnings)
        return;

    if (current_file)
        message_print(NIDL_LINEFILE, current_file, *yylineno_p);
    message_print(msg_id, arg1, arg2, arg3, arg4, arg5);

    if (++warnings > MAX_WARNINGS)
    {
        message_print(NIDL_MAXWARN, MAX_WARNINGS);
        nidl_terminate();
    }
}

/*
 *  s e t _ n a m e _ f o r _ e r r o r s
 *
 *  Function:   Records the name of the file being processed.  This name
 *              will be prepended onto error messages.
 *
 *  Inputs:     name - a pointer to the file name.
 *
 */

void set_name_for_errors
#ifdef PROTO
(
    char *filename
)
#else
(filename)
    char *filename;
#endif

{
    if (filename != NULL)
    {
        error_file_name_id = STRTAB_add_string(filename);
        STRTAB_str_to_string(error_file_name_id,&current_file);
    }
    else current_file = NULL;
}


/*
 *  i n q _ n a m e _ f o r _ e r r o r s
 *
 *  Function:   Returns the name of the file being processed.
 *
 *  Outputs:   name - the file name is copied through this.
 *
 */

void inq_name_for_errors
#ifdef PROTO
(
    char *name
)
#else
(name)
    char    *name;
#endif

{
    if (current_file)
        strcpy(name, current_file);
    else
        *name = '\0';
}
