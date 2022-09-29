/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/
/*-- macLibrary.h --------------
 *
 * This file contains as a group the modules from other DIT libraries required
 * to make the code run in freestanding mode. See macPort.[ch].
 *
 */

#include "macPort.h"


#ifndef MACLIBRARY_C
#define MACLIBRARY_C

/*-- DitError.h ------
  
   This object is used to trace DIT software. Error is reported at the point
   of origination of a non-DIT error, then traced using the trace call.
*/

#ifndef DITERROR_H
#define DITERROR_H

#include <stdlib.h>
#include <sys/errno.h>

#define E_NONE          0
#define E_OPEN          15001
#define E_CLOSE         15002
#define E_READ          15003
#define E_WRITE         15004
#define E_IOCTL         15005
#define E_MEMORY        15006
#define E_NOTFOUND      15007
#define E_FOUND         15008
#define E_NOTSUPPORTED  15009
#define E_RANGE         15010
#define E_ENVIRONMENT   15011
#define E_PERMISSION    15012
#define E_SYNTAX        15013
#define E_UNKNOWN       15014
#define E_MEDIUM        15015
#define E_FORMAT        15016
#define E_NOOBJECT      15017
#define E_DATA          15018
#define E_PROTECTION    15019
#define E_MOUNT         15020
#define E_UNMOUNT       15021
#define E_BLOCK         15022
#define E_SPACE         15023
#define E_CREATE        15024
#define E_UNLINK        15025
#define E_EMPTYLINK     15026
#define E_SOCKET        15027
#define E_MKDIR         15028
#define E_CHDIR         15029
#define E_RENAME        15030
#define E_USERSTOP      15031
#define E_SKIP          15032
#define E_CROSSLINK     15033
#define E_SELFLINK      15034
#define E_EXPAND        15035
#define E_TRUNCATE      15036
#define E_ENTRY         15037
#define E_BITMAP        15038
#define E_SLOCK         15039
#define E_HLOCK         15040
#define E_BADBLOCK      15041
#define E_UNRECOVER     15042
#define E_BTREEBMPNODE  15043
#define E_BTREENODESZ   15044


#define MAX_TRACE_NAME        31
#define MAX_ERROR_STRING      127

struct dit_error
   {
    int e_number;
    int e_errno;
    char e_module[ MAX_TRACE_NAME+1 ];
    char e_routine[ MAX_TRACE_NAME+1 ];
    char e_message[ MAX_ERROR_STRING+1 ];
    struct dit_error *e_next;
   };

extern int errno;
extern struct dit_error *dit_error_alloc();
extern char *get_error(void), *get_nice_error(void);
extern struct dit_error *get_error_list(void);
extern int set_error(int, char *, char *, char *, ... ),
           clear_error(void),
           init_error(void),
           set_trace(),
           get_number(void),
           dit_error_free();

#endif
/*-- end DitError.h -------------
 *
 */

/*-- dt.h ------
 *
 */

#ifndef DT_H
#define DT_H


#include <sys/param.h>
#include <ctype.h>

#define MAXPATHNAME MAXPATHLEN
#define MAXNAMLEN 255

/* UNIX errno */
extern int errno;

/* device macros */
#define DT_DEVICENONE      -1
#define DT_DEVICENEXT      0
#define DT_DEVICEDOSF      1
#define DT_DEVICEMACF      2
#define DT_DEVICEUNXF      3

#define DT_BLOCKSIZE       1024

/* directory de-select macros 
   (eventually will provide select info in high-order bytes */
#define DSELECT             0x0000FFFF /* and this for deselection flags */
#define DSEL_DOTANDDOTDOT   0x00000001
#define DSEL_ALLDOT         0x00000002

struct dt_file
   {
    /* permission */
    long            dtf_atime,              /* UNIX-format access date in seconds */
                    dtf_mtime,              /* UNIX-format mod date in seconds */
                    dtf_ctime;              /* UNIX-format status change date */
    char           *dtf_filename;           /* name */
    long            dtf_size;               /* size in bytes */
    long            dtf_resourcesize;       /* rfork size in bytes (mac only)*/
    int             dtf_is_directory;       /* is it a directory? */

    struct dt_file *dtf_next;
   };

struct dt_device
   {
    int dtv_devicetype; /* see device type macros */
   };

struct dt_openfile
   {
    int                 dto_is_open;        /* is this fd active? */
    int                 dto_devicefd;   /* fd returned by device-dependent code */
   };

struct dt_directory
   {
    char                  dtd_directoryname[ MAXNAMLEN+1 ];
    int                   dtd_entries; /* number of entries */

    /* first file in this directory */
    struct dt_file       *dtd_firstfile;
   };

extern int dt_dir_alloc(struct dt_directory **),
           dt_file_alloc(struct dt_file **),
           dt_file_free(struct dt_file **),
           dt_string_alloc(char *, char **),
           dt_string_free(char **),
           dt_dir_free(struct dt_directory **),
           dt_file_find(struct dt_file *, char *, struct dt_file **),
           dt_file_copy(struct dt_file **, struct dt_file *),
           dt_file_numfind(struct dt_file *, int, struct dt_file **);
	   int dt_list_copy(struct dt_file **, struct dt_file *);
#endif
/*-- end dt.h -----------
 *
 */

#endif
