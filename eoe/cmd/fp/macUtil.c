/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993
       All Rights Reserved
*/

/*
    DIT TransferPro macUtil.c -  General purpose Macintosh support functions.
*/

#include "macSG.h"
#include "dm.h"

/*--------------- au_to_partition_block --------------------------------
    Converts an allocation unit number to a physical block number.
------------------------------------------------------------------------*/

int au_to_partition_block (vib, unit, vstart)
struct m_VIB *vib;
unsigned int unit;
int vstart; /* volume start physical block in the partition */
{
    return ((unit * vib->drAlBlkSiz / SCSI_BLOCK_SIZE) +
                                 vib->drAlBlSt + vstart);
}


/*--------------- au_to_block -------------------------------------------
    Converts an allocation unit number to a physical block number.
------------------------------------------------------------------------*/

int au_to_block (vib, unit)
struct m_VIB *vib;
unsigned int unit;
{
    return ((unit * vib->drAlBlkSiz / SCSI_BLOCK_SIZE) + vib->drAlBlSt);
}

/*-- ditPathToMacPath -----------
 *
 */

char *ditPathToMacPath( pathname, macname )
    char *pathname,
         *macname;
   {
    char *retval = macname;
    int   in_backslash = 0;
    char *macstart = macname;

    while ( *pathname )
       {
        if ( in_backslash )
           {
            if ( *pathname == '/' ) /* no special interp for escaped chars */
               {
                *macname = *pathname;
                macname++;
                in_backslash = 0;
               }
            else
               {
                *(macname++) = '\\';
                if ( *pathname != '\\' )
                   {
                    in_backslash = 0;
                    *(macname++) = *pathname;
                   }
               }
           }
        else if ( *pathname == '\\' )
           {
            in_backslash = 1;
           }
        else if ( *pathname == ':' )
           {
            ;
           }
        else if ( *pathname == '/' )
           {
            if ( macname == macstart ||
                (*(macname-1) != '/' && *(pathname+1)) )
               {
                *macname = ':';
                macname++;
               }
           }
        else
           {
            *macname = *pathname;
            macname++;
           }
        pathname++;
       }
    if ( in_backslash )
       {
        *(macname++) = '\\';
       }
    *macname = 0;

    return( retval );
   }

/*-- macPathToDitPath ---------
 *
 */

char *macPathToDitPath( macPath, ditPath )
    char *macPath,
         *ditPath;
   {
    char *retval = ditPath;

    while ( *macPath )
       {
        if ( *macPath == '/' )
           {
            *(ditPath++) = '\\';
            *(ditPath++) = *macPath;
           }
        else if ( *macPath == ':' )
           {
            *(ditPath++) = '/';
           }
        else
           {
            *(ditPath++) = *macPath;
           }
        macPath++;
       }
    *ditPath = 0;

    return( retval );
   }

void splitPathSpec ( pathspec, path, filename )
char *pathspec;
char *path;
char *filename;
   {
    char *ptr;

    if ( (ptr = strrchr (pathspec, '/')) != (char *)0)
       {
        strcpy ( filename, ptr+1);
        strncpy ( path, pathspec, ptr - pathspec);
        *(path + (ptr-pathspec)) = '\0';
       }
    else
       {
        *path = '\0';
        strcpy ( filename, pathspec );
       }
   }

int macGetPath ( volume, path, fullPath )
struct m_volume *volume;
char *path;
char *fullPath;
   {
    char *ptr;
    int retval = E_NONE;
    char buffer[MAXPATHLEN+1];
    char macpath[MAXPATHLEN+1];
    char tempbuf[MAXPATHLEN+1];
    char *bufptr;

    
    ditPathToMacPath (path, macpath);
    if ( *macpath == ':' )
       {
        ditPathToMacPath (volume->mountPoint, tempbuf);
        if ( !*tempbuf || !*(tempbuf+1) )
           {
            strcpy( fullPath, macpath );
           }
        else
           {
            strcpy(fullPath, macpath+strlen(tempbuf) );/* full path was given */
           }
        if ( !*fullPath )
           {
            strcpy( fullPath, ":" );
           }
       }
    else
       {
        strcpy (fullPath, volume->curDir);
        strcpy (buffer, macpath);
        bufptr = buffer;
        bufptr = strtok (bufptr, ":");
        while ( retval == E_NONE && bufptr )
           {
            if ( !strcmp (bufptr, "..") )
               {
                /* go up a level */
                if ( !strcmp (fullPath, ":") )
                   {
                    retval = set_error (E_SYNTAX, "macUtil", "macGetPath", 
                                       "Incorrect path name syntax");
                   }
                else
                   {
                    ptr = strrchr (fullPath, ':');
                    *ptr = '\0';
                   }
               }
            else if ( !strcmp (bufptr, ".") )
               {
                /* current directory */
                ;
               }
            else
               {
                if ( *fullPath && *(fullPath + strlen(fullPath) - 1) != ':')
                   {
                    strcat (fullPath, ":");
                   }
                strcat (fullPath, bufptr);
               }
            bufptr = strtok ((char *)0, ":");
           }
       }

    return (retval);
   }
