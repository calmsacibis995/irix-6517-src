//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          localedef.c
//
// DESCRIPTION :        main program file and entry point for localedef
//
//____________________________________________________________________________

#include "localedef.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <locale.h>
#include "tables.h"       /* Table function and data struct declarations */ 
#include "collate.h"
#include "y.tab.h"       /* Generated from yacc compilation of loc.y */
     
/* Externally declared variables */
extern FILE *yyin;
extern ctype_width;

/* Internal function prototypes */
void process_command_line(int, char **);
void print_usage_and_exit(void);
void open_files(void);
void set_file_mode(FILE *);

typedef enum { createFileOp, readFileOp, writeFileOp, deleteFileOp } FileOp;
void file_error_and_exit( const char * filename, FileOp operation );

void write_default_numeric ( FILE * );

void post_process ( int cat );
void post_process_copy ( int category );
void fork_tbl_handler(char * bin, char *arg, char * filename);
void exec_mkmsgs ( );
void write_iconv_spec ( FILE * );
int  exec_iconv ( );


/* Module Globals */

FILE  *input_file   = NULL;         
FILE  *output_file  = NULL;        
FILE  *charmap_file = NULL;       

FILE *att_file [ LC_ALL ] = { 0,0,0,0,0,0 };
FILE *iconv_spec_file = NULL;

char *output_filename   = NULL;
int wflag = 0; /* permission to write ICONV_PATH */

/*
 * These flags will be set when the associated output files
 * are created and filled with meaningful content;  they are
 * used when the system decides to fork off the table processing
 * programs.
 */
int has_content [ LC_ALL ] = { 0,0,0,0,0,0 };

/* Name of the locale being created */
char *locale_name;

/* Catalog id */
nl_catd catid;

/* Static Globals */
static int force_flag = 0; 

static char *input_filename    = NULL;
static char *charmap_filename  = NULL;
static char *iconv_spec_filename = NULL;

char * att_filename [ LC_ALL ] = { 0,0,0,0,0,0 };
char * att_filename_format [ LC_ALL ] = {
    "%s/chrtbl_ctype_%s",
    "%s/chrtbl_numeric_%s",
    "%s/LC_TIME",
    "%s/colltbl_%s",
    "%s/montbl_%s",
    "%s/nl_lang_%s",
};

const char * binary_filename [ LC_ALL ] = {
    "LC_CTYPE",
    "LC_NUMERIC",
    "LC_TIME",
    "LC_COLLATE",
    "LC_MONETARY",
    "LC_MESSAGES/Xopen_info"
};

/* Handling of the copy directive */
char * symlink_copy_to [ LC_ALL ] = { 0,0,0,0,0,0 };
static int symlink_copy_flag = 0;
#define ENV_SYMLINK_COPY "_LOCALEDEF_SYMLINK_COPY"

/*  Constants */
#define ATT_LCTIME_SRC   "LC_TIME"
#define ICONV_SPEC_SRC  "iconv.spec."
#define XOPEN_INFO_FILE "Xopen_info"	

#define LOCALE_PATH	"/usr/lib/locale/"
#define ICONV_PATH	"/usr/lib/iconv/spec"
#define ICONV_PATH_LEN  (sizeof(ICONV_PATH)+1)
#define ICONV_COMP_BIN  "/usr/sbin/iconv_comp"
#define MKMSGS_BIN 	"mkmsgs"

/* Debug */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#ifndef LEXDEBUG
#define LEXDEBUG 0
#endif
extern int yydebug;

int
main ( int argc, char *argv[] )
{
    struct stat buf;
    char *ctype_prog, *fname;
    int cat;

    if ( setlocale ( LC_ALL, "" ) == NULL )
	/* Note: no need to take trouble to localize this message.
	   We failed to setlocale so we are in C locale. Therefore, the
	   correct localized catalog cannot be opened */
        fprintf ( stderr, "Warning: locale not supported by C library\n" );

    /* Failure to open the catalog will result in using the
       english default messages */
    catid = catopen ( "localedef.cat", NL_CAT_LOCALE );
   
    process_command_line(argc, argv);

   /*
    * Open all files necessary for processing.  This includes the optional locale input and
    * charmap files, the 'dummy' output file to accommodate the POSIX localedef signature, and 
    * the ATT source files that will be generated.
    *
    * The temporary files needed for building the charsym, collsym and collelement tables are
    * opened elsewhere.
    */
   
    open_files();

   /* 
    * Determine if the input or charmap files are empty
    */
   
    if (input_filename != NULL)
       {
       if ((stat(input_filename, &buf) != -1) &&
	   (buf.st_size == 0))
	   return(0);
       }
    if (charmap_filename != NULL)
       {
       if ((stat(charmap_filename, &buf) != -1) &&
	   (buf.st_size == 0))
	   return(0);
       }
   
   /* 
    * Preprocess the input files so that comment characters,
    * escape characters, and embedded brackets in special symbols
    * can be handled uniformly.
    */

    if (lpp(&input_file, LOC) != 0)
       exit(3);
   
    if (lpp(&charmap_file, CM) != 0)
       exit(2);
   
   /* 
    * Read the charmap file
    */
   process_charmap ( charmap_file );

#ifdef YACC_DEBUG   
    /* XXX TABLE TESTS FOR DEBUGGING */
    dump_list(charmap_list, "charmap");
    dump_list(collsym_list, "collating-symbol");
    dump_list(collelt_list, "collating-element");
#endif
   
   /*
    * Set the input file for the lexer.
    */
    if (input_file != NULL)
        yyin = input_file;
    
   /*
    * As the POSIX locale source file is parsed, ATT locale source files are produced.
    * These are named chrtbl_src (source for the chrtbl utility), montbl_src (source 
    * for the montbl utility) and colltbl_src (source for the colltbl utility).
    */
    yydebug = YYDEBUG;
    yyparse();

    /* If no numeric section was defined, write default values */
    if ( ! has_content [ LC_NUMERIC ] )
    {
	write_default_numeric ( att_file [ LC_NUMERIC ] );

	/* Required to get the post processing done */
	has_content [ LC_NUMERIC ] = 1;
    }

    /* Make sure LC_MESSAGES directory there */
    if ( has_content [ LC_MESSAGES ] || symlink_copy_to [ LC_MESSAGES ] )
    {
	char path [ PATH_MAX ];
	sprintf ( path, "%s/LC_MESSAGES", output_filename );

	if ( access ( path, W_OK ) )
	    if ( errno != ENOENT || mkdir ( path, 0755 ) )
		file_error_and_exit ( path, createFileOp );
    }

    for ( cat = LC_CTYPE; cat < LC_ALL; cat ++ )
	post_process ( cat );

    if ( wflag )
    {
	write_iconv_spec ( iconv_spec_file );
	fclose ( iconv_spec_file );
	exec_iconv();
    }
    else
	unlink ( iconv_spec_filename ); /* remove the file if it's empty */
}  


/*
 * XXX Should look into using getopt() here!
 */
void process_command_line(int argc, char **argv)
{
    int optind = 1;
    unsigned size;
    char *pathoutp;
    char *c;

    if (argc < 2)
        print_usage_and_exit();

    while (optind < argc) {

        if (strcmp(argv[optind], "-c") == 0) {
            force_flag = 1;
            optind++;
            continue;
        }
        if (strcmp(argv[optind], "-i") == 0) {
            if (++optind <= (argc - 2))
                input_filename = argv[optind++];
            else
                print_usage_and_exit();
            continue;
        }
        if (strcmp(argv[optind], "-f") == 0) {
            if (++optind <= (argc - 2))
                charmap_filename = argv[optind++];
            else
                print_usage_and_exit();
            continue;
        }

        if (strncmp(argv[optind], "-", 1) == 0)
            print_usage_and_exit();
        if (optind == (argc - 1))
             output_filename = argv[optind];
		if(access(ICONV_PATH, W_OK) == 0 )
		  wflag++; /* has permission to write to /usr/lib/iconv/spec */
        optind++;
    }

    if (output_filename == NULL)
        print_usage_and_exit();


    if ( ( c= getenv ( ENV_SYMLINK_COPY ) ) &&
	 ( atoi(c) != 0 ) )

	symlink_copy_flag ++;

}  


/*
 * Need to put this in a message catalog.
 */
void print_usage_and_exit()
{
    fprintf(stderr, GETMSG ( MSGID_USAGE ) );
    exit(4);
}  

void file_error_and_exit( const char * filename, FileOp operation )
{
    char *error_msg = strerror ( errno ); /* Required because gettxt may change errno */
    char *msg;

    switch ( operation ) {
    case createFileOp:
	msg = GETMSG (  MSGID_FILE_CREATE );
	break;

    case readFileOp:
	msg = GETMSG (  MSGID_FILE_READ );
	break;

    case writeFileOp:
	msg = GETMSG (  MSGID_FILE_WRITE );
	break;

    case deleteFileOp:
	msg = GETMSG (  MSGID_FILE_DELETE );
	break;

    default:
	msg = GETMSG ( MSGID_FILOP );
    }

    fprintf ( stderr, msg,
	      filename, error_msg );

    exit ( 4 );
}

void open_files()
{
    char path [ PATH_MAX ];
    int cat;
    FILE * file;

    errno = 0;

    /* Input file */
    if (input_filename != NULL) 
    {
	strcpy ( path, input_filename );
	if ( (input_file = fopen ( path , "r")) == (FILE *) NULL)
	    file_error_and_exit ( path, readFileOp );
    }
    else
	input_file = stdin;

    /* Charmap file */
    if (charmap_filename != NULL)
	strcpy ( path, charmap_filename );
    else
	strcpy ( path, DEFAULT_CM_FILENAME );

    if ((charmap_file = fopen( path , "r")) == (FILE *) NULL)
	file_error_and_exit ( path, readFileOp );

    /* Various output files */
    locale_name = strdup ( basename ( output_filename ) );
    
    /* Check directory status */
    if ( access ( output_filename, W_OK ) )
    {
	if ( errno != ENOENT || mkdir ( output_filename, 0755 ) )
	    file_error_and_exit ( output_filename, writeFileOp );
    }

    for ( cat = LC_CTYPE; cat < LC_ALL; cat ++ )
    {
	/* Remove ATT target binary files */
	sprintf ( path, "%s/%s", output_filename, binary_filename [ cat ] );
	if ( unlink ( path ) && errno != ENOENT )
	    file_error_and_exit ( path, deleteFileOp );

	sprintf ( path, att_filename_format [ cat ], output_filename, locale_name );
	if ( ( file = fopen ( path, "w" ) ) == NULL )
	    file_error_and_exit ( path, createFileOp );

	fchmod ( fileno ( file ), 0755 );
	att_filename [ cat ] = strdup ( path );
	att_file     [ cat ] = file;
    }

    if (wflag) {
	sprintf ( path, "%s/%s%s", 
		  ICONV_PATH, ICONV_SPEC_SRC, locale_name);
	if ( ( file = fopen ( path, "w" ) ) == NULL )
	    file_error_and_exit ( path, createFileOp );

	fchmod ( fileno ( file ), 0755 );
	iconv_spec_filename = strdup ( path );
	iconv_spec_file = file;
    }
    
    return;
}  

void exec_mkmsgs ( )
{
    int pid;
    char out [ PATH_MAX ];
    
    /* output_filename contains the location of the new locale, ie the
     * last argument to the command (either absolute or relative).
     * binary_filename contains LC_MESSAGES/Xopen_info. So, out
     * contains the location of the target message catalog from the
     * cwd.  */

    sprintf ( out, "%s/%s", output_filename,
	      binary_filename [ LC_MESSAGES ] );
   
    if ((pid = fork()) < 0)
	fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
    
    else if (pid == 0) /* child process */
    {
	if ( (execlp(MKMSGS_BIN, MKMSGS_BIN, "-o",
		     att_filename [ LC_MESSAGES ],
		     out,  (char *) 0)) < 0 ) 
	    fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
    }
    if (waitpid(pid, NULL, 0) < 0)
	fprintf(stderr, GETMSG ( MSGID_WAITERR ) );
    return;
}

           


void fork_tbl_handler(char *bin, char *arg, char *filename)
{
   int pid;
   char *fname = NULL;
   
   if ((pid = fork()) < 0)
      fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
   else if (pid == 0) /* child process */
   {
       if (chdir(output_filename) != 0)
	   fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
       
       fname = strrchr(filename, SLASH);
       
       if ( arg )
       {
	   if (execlp(bin, bin, arg, fname+1, (char *) 0) < 0)
	       fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
       }
       else
       {
	   if (execlp(bin, bin, fname+1, (char *) 0) < 0)
	       fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
       }
       /* Won't be reached */
   }
   printf ( GETMSG ( MSGID_RUNCMD ), bin );
   
   if (waitpid(pid, NULL, 0) < 0)
      fprintf(stderr, GETMSG ( MSGID_WAITERR ) );
   
   printf ( GETMSG ( MSGID_RUNDONE ) );
   return;
}

int     exec_iconv( void )
{
    DIR *               dir;
    struct direct *     dirent;
    int                 exec_argc = 0 ;
    char *              exec_argv [ ARG_MAX ];
    pid_t               pid;

    if ( !( dir = opendir ( ICONV_PATH) ) )
	fprintf(stderr, GETMSG ( MSGID_FILOP ),
		ICONV_PATH, strerror ( errno ) );
    exec_argv [ exec_argc ++ ] = ICONV_COMP_BIN ;

    while ( dirent = readdir ( dir ) )
        if ( strstr ( dirent->d_name, "spec" ) )
            exec_argv [ exec_argc ++ ] = strdup ( dirent->d_name );

    exec_argv [ exec_argc ++ ] = NULL;

    closedir ( dir );

    if ( ( pid = fork() ) < 0 )
    {
        /* Could not fork */
        fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
        return -1;
    }
    else if ( pid == 0 )
    {
        /* Child process */
        chdir ( ICONV_PATH );
        if ( execv ( exec_argv[0], exec_argv ) )
        {
            /* Could not execute */
	    fprintf(stderr, GETMSG ( MSGID_FORKERR ) );
            return -1;
        }
	/* Won't be reachead */
    }

    printf ( GETMSG ( MSGID_RUNCMD ), "iconv_comp" );

    if ( waitpid ( pid, NULL, 0 ) < 0 )
    {
	fprintf(stderr, GETMSG ( MSGID_WAITERR ) );
        return -1;
    }
    printf ( GETMSG ( MSGID_RUNDONE ) );

    return 0;
}


/* Write to the chrtbl source file characters that are always
   automatically included in a character class */
void write_posix_ctype ( FILE * att_lcctype_src )
{
    fputs ( "LC_CTYPE\tLC_CTYPE\n"
	    "#\n"
	    "# Below is added for POSIX compliance in the automatically-included area.\n"
	    "#\n"
	    "isupper\t\t0x41 - 0x5a\n"
	    "islower\t\t0x61 - 0x7a\n"
	    "isdigit\t\t0x30 - 0x39\n"
	    "isspace\t\t0x20 0x9 - 0xd\n"
	    "isblank\t\t0x20 0x9\n"
	    "isprint\t\t0x20\n"
	    "isxdigit\t0x30 - 0x39\t0x61 - 0x66\t 0x41 - 0x46\n"
	    "#\n"
	    "# End of the automatically-include section.\n", att_lcctype_src );
}


/* Write the default LC_NUMERIC section in chrtbl input file */
void write_default_numeric ( FILE * att_lcctype_src )
{
    fputs ( "LC_NUMERIC\tLC_NUMERIC\n"
	    "#\n"
	    "# Default numeric section\n"
	    "#\n"
	    "decimal_point\t.\n"
	    "thousands_sep\t0x0\n"
	    "#\n"
	    "# End of default numeric section\n", att_lcctype_src );
}


void write_iconv_spec ( FILE * iconv_spec_src )
{
    fprintf ( iconv_spec_src,
	      "# iconv specification file generated by localedef\n"
	      "# for locale %s\n"
	      "locale_codeset \"%s\"\t\"%s\";\n",
	      locale_name, locale_name, charmap_info.code_set_name );
}

void
exit_malloc_error ( )
{
    fprintf ( stderr, GETMSG(MSGID_NOMEM) );
    exit ( 4 );
}


FILE *
process_symlink_copy ( int cat, const char * from )
{
    /* Check for locale existance */
    char path[PATH_MAX];
    struct stat sbuf;
    FILE * src = NULL;

    /* If name doesn't contain slashes, prepend /usr/lib/locale */
    if ( ! strchr ( from, '/' ) )
	sprintf ( path, "%s%s", LOCALE_PATH, from );
    else
	strcpy ( path, from );

    /* stat the file */
    if ( stat ( path, &sbuf ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_FILE_READ ),
		  path, strerror (errno) );
	return NULL;
    }

    if ( S_ISREG(sbuf.st_mode) )
    {
	/* Its a file. Assuming its a source file */
	if ( ! ( src = fopen ( path, "r" ) ) )
	{
	    fprintf ( stderr, GETMSG ( MSGID_FILE_READ ),
		      path, strerror (errno) );
	    return NULL;
	}

	/* The dirname of that file is assumed to be the location of
	   the binaries */
	strcpy ( path, dirname ( path ) );
    }

    strcat ( path, "/" );
    strcat ( path, binary_filename [cat] );

    if ( symlink_copy_flag )
	symlink_copy_to [ cat ] = strdup ( path );

    return src;
}

void
post_process_copy ( int cat )
{
    char target [ PATH_MAX ];

    /* Remove att source file */
    unlink ( att_filename [ cat ] );

    /* Establish target name */
    sprintf ( target, "%s/%s", output_filename, binary_filename [ cat ] );
    
    /* Symlink target to source (target should have already been removed */
    if ( ( unlink (target) && errno != ENOENT ) ||
	 symlink ( symlink_copy_to [ cat ], target ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_FILE_CREATE ),
		  target, strerror (errno) );
	return;
    }
}

void
post_process ( int cat )
{
    char path [ PATH_MAX ];
    char * program;
    char * arg = 0;

    fclose ( att_file [ cat ] );

    /* Handle copy clause */
    if ( symlink_copy_to [ cat ] )
    {
	post_process_copy ( cat );
	return;
    }

    if ( cat == LC_TIME )
	/* Time category doesn't need processing */
	return;

    /* If nothing has been defined in that file,
     * remove it */
    if ( ! has_content [ cat ] )
    {
	unlink ( att_filename [ cat ] );
	return;
    }
    
    switch ( cat ) {
    case LC_CTYPE:
	/* execute chrtbl or wchrtbl */
	if (ctype_width == 1)
	    program = "chrtbl";
	else
	    program = "wchrtbl";
	break;

    case LC_NUMERIC:
	program = "chrtbl";
	arg = "-s";
	break;

    case LC_MONETARY:
	program = "montbl";
	break;
	
    case LC_COLLATE:
	program = "colltbl";
	break;

    case LC_MESSAGES:
	exec_mkmsgs ();
	return;

    case LC_TIME:
    default:
	/* Nothing to do */
	return;
    }

    fork_tbl_handler ( program, arg, att_filename [ cat ] );
}
