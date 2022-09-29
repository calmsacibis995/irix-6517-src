/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/
/*-- TransferPro macLibrary.c --------------
 *
 * This file contains as a group the modules from other DIT libraries required
 * to make the code run in freestanding mode. See macPort.[ch].
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include "macLibrary.h"

static char *Module = "macLibrary";


/*-- DitError.c ------------------
 *
 */

static struct dit_error *ErrorStack = (struct dit_error *)0;
static struct dit_error LastError;
static int UseStack = 0;

/*---- set_error --------------------------------------------

   Logs an error. If init_error() has been called, the error
   is added to the error stack.

-------------------------------------------------------------*/   

int set_error( int number, char *module, char *funcname, char *formatstr, ... )
   {
    struct dit_error *tmp = (struct dit_error *)0;
    va_list ap;

    LastError.e_number = number;
    LastError.e_errno = errno;
    strncpy( LastError.e_module, module, MAX_TRACE_NAME );
    *(LastError.e_module+MAX_TRACE_NAME) = 0;
    strncpy( LastError.e_routine, funcname, MAX_TRACE_NAME );
    *(LastError.e_routine+MAX_TRACE_NAME) = 0;

    va_start(formatstr, ap);
    vsprintf( LastError.e_message, formatstr, ap );
    va_end(ap);

    LastError.e_next = (struct dit_error *)0;

    /* Add to error stack if error-stacking was requested. */
 
    if (UseStack)
       {
        tmp = ErrorStack;
        ErrorStack = (struct dit_error *)malloc (sizeof (struct dit_error) );
        if (ErrorStack == (struct dit_error *)0)
           {
            /* out of memory... how do we handle this?? */
           }
        else
           {
            ErrorStack->e_number = LastError.e_number;
            ErrorStack->e_errno = LastError.e_errno;
            strcpy (ErrorStack->e_module, LastError.e_module);
            strcpy (ErrorStack->e_routine, LastError.e_routine);
            strcpy (ErrorStack->e_message, LastError.e_message);
            ErrorStack->e_next = tmp;
           }
       }
    else
       {
        ErrorStack = &LastError;
       }

    return( number );
}



/*-- get_error -------
 *
 *   Returns the current error stack as a pointer to a formatted string
 *   if error(s) have been logged; returns null pointer otherwise.
 *
 *   Note: The application calling this function needs to free
 *         the returned string when it is done with it.
 *
 */

char *get_error( )
   {
    char *errbuf = (char *)0;
    struct dit_error *err;
    int curlen;


    if ( ErrorStack )
       {
        if ( (errbuf = (char *)malloc((unsigned)(30+
              strlen(ErrorStack->e_module)+
              strlen(ErrorStack->e_routine)+
              strlen(ErrorStack->e_message))))
              == (char *)0 )
           {
            ;
           }

        else
           {
            err = ErrorStack;
            *errbuf = '\0';
            while ( err  &&  errbuf )
               {
                curlen = strlen (errbuf);
                if ( (errbuf = (char *)realloc(errbuf,
                    (unsigned)(curlen + 30 +
                    strlen(err->e_module)+
                    strlen(err->e_routine)+
                    strlen(err->e_message))))
                    == 0 )
                   {
                    ;
                   }
                else
                   {
                    sprintf( errbuf+curlen, "<%d, %d, %s, %s, %s>%c",
                        (struct dit_error *)err->e_number,
                        (struct dit_error *)err->e_errno,
                        (struct dit_error *)err->e_module,
                        (struct dit_error *)err->e_routine,
                        (struct dit_error *)err->e_message,
                        ( err->e_next ) ? ',' : ' ' );
                    err = err->e_next;
                   }
               }
           }
       }


    return( errbuf );
   }


/*-- get_error_list ----------
 *
 *   Returns the current error stack.
 */

struct dit_error *get_error_list( )
   {
    return (ErrorStack);
   }



/*-- get_nice_error -----
 *
 *   Returns the current error data as a "nice" error message.
 *
 *   Note: The application calling this function needs to free
 *         the returned string when it is done with it.
 *
 */

char *get_nice_error( )
   {
    char *retval;
 
    retval = get_error();     /* Same as get_error for now. */

    return( retval );
   }


/*-- clear_error ------
 *
 *   Clears the current error stack; freeing all associated memory.
 */

int clear_error()
   {

    struct dit_error *err, *tmp;

    if ( UseStack )
       {
        /* Free the error stack */

        err = ErrorStack;
        while (err)
           {
            tmp = err->e_next;
            free ((char *)err);
            err = tmp;
           }
       }

    ErrorStack = (struct dit_error *)0;
    return ( E_NONE );
   }

/*-- get_number ----------
 *
 *   Returns the number of the last error that was logged.
 */

int get_number( )
   {
    int retval = E_NONE;

    if ( ErrorStack )
       {
        retval = ErrorStack->e_number;
       }

    return (retval);
   }

/*-- init_error -----
 *
 *   Initializes the error handling functions so that errors can be
 *   stacked. 
 *
 *   NOTE: it is the applications responsibility to use the clear_error
 *   function to clear the error stack when errors have been handled.
 */

int init_error( )
   {

    UseStack = 1;

    return( E_NONE );
   }


/*-- end DitError.c --------------------
 *
 */


/*-- dt_string_alloc ----
 *
 */

int dt_string_alloc( value, ptr )
    char *value;
    char **ptr;
   {
    int  retval = E_NONE;
    char *funcname = "dt_string_alloc";

    if ( (*ptr = malloc((unsigned)(strlen(value)+1))) == (char *)0 )
       {
        retval = set_error( E_MEMORY, Module, funcname, "" );
       }
   
    return( retval );
   }

/*-- dt_string_free ----
 *
 */

int dt_string_free( ptr )
    char **ptr;
   {
    int  retval = E_NONE;

    if ( ptr && *ptr )
       {
        free( *ptr );
        *ptr = 0;
       }
  
    return( retval );
   }

/*-- dt_dir_alloc -----
 *
 */

int dt_dir_alloc( dirptr )
    struct dt_directory **dirptr;
   {
    int retval = E_NONE,
        a;
    char *funcname = "dt_dir_alloc";

    if ( (*dirptr = (struct dt_directory *)malloc(
       (unsigned)sizeof(struct dt_directory))) == (struct dt_directory *)0 )
       {
        retval = set_error( E_MEMORY, Module, funcname, "" );
       }
    else
       {
        for( a = 0; a < sizeof(struct dt_directory); a++ )
           {
            *((char *)*dirptr + a)= 0;
           }
       }
    
    return( retval );
   }

/*-- dt_dir_free ----
 *
 */

int dt_dir_free( dirptr )
    struct dt_directory **dirptr;
   {int    retval = E_NONE;

    if ( dirptr && *dirptr )
       {
        if ( (*dirptr)->dtd_firstfile )
           {
            retval = dt_file_free( &(*dirptr)->dtd_firstfile );
           }
        free( *dirptr );
        *dirptr = 0;
       }  

    return( retval );
   }

/*-- dt_list_copy ----
 *
 */

int dt_list_copy( result, input )
    struct dt_file **result,
                    *input;
   {int    retval = E_NONE;

    *result = 0;

    if ( input && (retval = dt_file_copy(result,input)) == E_NONE )
       {
        if (input->dtf_next )
           {
            retval = dt_list_copy( &((*result)->dtf_next), input->dtf_next );
           }
       }
  
    return( retval );
   }

/*-- dt_file_copy -------
 *
 * This function doesn't copy a list; write dt_list copy if you want that.
 *
 */

int dt_file_copy( result, input )
    struct dt_file **result,
                    *input;
   {int              retval = E_NONE;
    char            *funcname = "dt_file_copy";

    *result = 0;
    if ( input )
       {
        if ( (retval = dt_file_alloc(result)) != E_NONE )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else if ( memcpy(*result,input,sizeof(struct dt_file)) == 0 ||
            (retval = dt_string_alloc(input->dtf_filename,
            &(*result)->dtf_filename)) != E_NONE )
           {
            dt_file_free( result );
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else
           {
            strcpy( (*result)->dtf_filename, input->dtf_filename );
            (*result)->dtf_next = 0;
           }
       }

    return( retval );
   }

/*-- dt_file_alloc ----
 *
 */

int dt_file_alloc( fileptr )
    struct dt_file **fileptr;
   {int    retval = E_NONE;
    char *funcname = "dt_file_alloc";
 
    if ( (*fileptr = (struct dt_file *)malloc((unsigned)sizeof(struct dt_file)))
        == (struct dt_file *)0 )
       {
        retval = set_error( E_MEMORY, Module, funcname, "" );
       }
    else
       {
        (*fileptr)->dtf_next = (struct dt_file *)0;
       }
    
    return( retval );
   }

/*-- dt_file_free ----
 *
 */

int dt_file_free( fileptr )
    struct dt_file **fileptr;
   {
    int  retval = E_NONE;

    if ( fileptr && *fileptr )
       {
        if ( (*fileptr)->dtf_next )
           {
            if ( (retval = dt_file_free(&(*fileptr)->dtf_next)) != E_NONE )
               {
                ;
               }
           }
        if ( retval == E_NONE )
           {
            if ( (*fileptr)->dtf_filename )
               {
                if ( (retval = dt_string_free(&(*fileptr)->dtf_filename)) != E_NONE )
                   {
                    ;
                   }
                free( *fileptr );
                *fileptr = 0;
               }
           }
       }
  
    return( retval );
   }


/*-- dt_file_insert ----
 *
 */

int dt_file_insert( filename, is_directory, listptr )
    char *filename;
    int  is_directory;
    struct dt_file **listptr;
   {
    int retval = E_NONE;
    int cmpval;
    struct dt_file *temp_file;
    char *funcname = "dt_file_insert";

    if ( (retval = dt_file_alloc(&temp_file)) != E_NONE )
       {
        ;
       }
    else 
       {
        while ( *listptr &&
            (cmpval = strcmp(filename,(*listptr)->dtf_filename)) < 0 )  
           {
            listptr = &(*listptr)->dtf_next;
           }
        if ( cmpval == 0 )
           {
            retval = set_error( E_FOUND, Module, funcname, "" );
           }
        else
           {
            if ( *listptr )
               {
                temp_file->dtf_next = *listptr;
               }
            *listptr = temp_file;
            if ( (retval = dt_string_alloc(filename,&temp_file->dtf_filename))
                != E_NONE )
               {
                ;
               }
            else
               {
                strcpy( temp_file->dtf_filename, filename );
                temp_file->dtf_is_directory = is_directory;
               }
           }
      }

    /* free objects if error */
    if ( retval != E_NONE )
       {
        dt_file_free( &temp_file );
       }

    return( retval );
   }

/*-- dt_file_remove ----
 *
 */

int dt_file_remove( filename, listptr )
    char   *filename;
    struct dt_file **listptr;
   {int retval = E_NONE;
    char *funcname = "dt_file_remove";

    while ( *listptr && strcmp(filename,(*listptr)->dtf_filename) != 0 )
       {
        listptr = &(*listptr)->dtf_next;
       }
    if ( *listptr )
       {
        retval = dt_file_free( listptr );
       }
    else
       {
        retval = set_error( E_NOTFOUND, Module, funcname, "" );
       }

    return( retval );
   }

/*-- dt_file_find ----
 *
 */

int dt_file_find( listptr, filename, output )
    struct dt_file *listptr;
    char   *filename;
    struct dt_file **output;
   {
    int retval = E_NONE;
    char *funcname = "dt_file_find";

    while ( listptr && strcmp(listptr->dtf_filename,filename) != 0 )
       {
        listptr = listptr->dtf_next;
       }
    if ( listptr )
       {
        *output = listptr;
       }
    else
       {
        retval = set_error( E_NOTFOUND, Module, funcname, "" );
       }

    return( retval );
   }

/*-- dt_file_numfind -----
 *
 */

int dt_file_numfind( listptr, number, output )
    struct dt_file *listptr;
    int number;
    struct dt_file **output;
   {
    int retval = E_NONE;

    do
       {
        *output = listptr;
       } while ( (listptr = (listptr)? listptr->dtf_next : listptr) && number-- > 0 );

    return( retval );
   }

/*-- end dt.c -------------
 *
 */


/*-- straicmp ----
 *
 * Case insensitive compare when char in each string is alphabetic,
 * else sensitive
 *
 */

int straicmp( s1, s2 )
    char *s1;
    char *s2;
   {
    while ( *s1 && *s2 &&
        ((isalpha(*s1)&&isalpha(*s2))?tolower(*s1):*s1) ==
        ((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) )
       {
        s1++;
        s2++;
       }

    return( (*s1)?
             (
              (*s2)?
                ((isalpha(*s1)&&isalpha(*s2))?
                tolower(*s1):*s1)-((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) :
                ((isalpha(*s1)&&isalpha(*s2))?tolower(*s1):*s1)
             )
           : (
              (*s2)?
                -((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) :
                0
             )
          );
   }

/*-- strnaicmp ----
 *
 */

int strnaicmp( s1, s2, n )
    char *s1;
    char *s2;
    int n;
   {
    int a = 0;

    while ( a < n &&
        *s1 &&
        *s2 &&
        ((isalpha(*s1)&&isalpha(*s2))?tolower(*s1):*s1) ==
        ((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) )
       {
        s1++;
        s2++;
        a++;
       }
    /* Am adding to put '_fn' and '1_fn' in correct position on Mac disk */
    if ( isdigit(*s1) && isalpha(*s2) )
       {
        return (*s1 - tolower(*s2) );
       }
    else if ( isalpha(*s1) && isdigit(*s2) )
       {
        return (tolower(*s1) - *s2 );
       }
    else if ( *s1 < '0' || *s2 < '0' )
       {
        ;
       }
    else if ( !isalpha(*s1) && isalpha(*s2) )
       {
        return ( tolower(*s2) - *s1);
       }
    else if ( isalpha(*s1) && !isalpha(*s2) )
       {
        return (*s2 - tolower(*s1));
       }
 
    /* Else old stuff */
    return( (a < n)?
               (
                (*s1)?
                   (
                    (*s2)? ((isalpha(*s1)&&isalpha(*s2))?tolower(*s1):*s1) - 
                    ((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) :
                    ((isalpha(*s1)&&isalpha(*s2))?tolower(*s1):*s1)
                   )
                 : (
                    (*s2)?
                    -((isalpha(*s1)&&isalpha(*s2))?tolower(*s2):*s2) :
                    0
                   )
               )
             : 0
         );
   }

int Nstrnicmp( s1, s2, n )
    char *s1;
    char *s2;
    int n;
   {
    int a = 0;

    while ( a < n && *s1 && *s2 && tolower(*s1) == tolower(*s2) )
       {
        s1++;
        s2++;
        a++;
       }

    return( (a < n)?
               ((*s1)?
                   ((*s2)? tolower(*s1)-tolower(*s2) : tolower(*s1))
                 : ((*s2)? -tolower(*s2) : 0))
             : 0 );
   }

int Nstricmp( s1, s2 )
    char *s1;
    char *s2;
   {
    while ( *s1 && *s2 && tolower(*s1) == tolower(*s2) )
       {
        s1++;
        s2++;
       }

    return( (*s1)?
               ((*s2)? tolower(*s1)-tolower(*s2) : tolower(*s1))
             : ((*s2)? -tolower(*s2) : 0) );
   }
