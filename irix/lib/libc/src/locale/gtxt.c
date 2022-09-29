/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/gtxt.c	1.7"

/* __gtxt(): Common part to gettxt() and pfmt()	*/

#ifdef __STDC__	
	#pragma weak setcat = _setcat
#endif
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <pfmt.h>
#include "_locale.h"
#include <unistd.h>
#include <errno.h>
#include <nl_types.h> 	/* MK - used by cat_name for the NLSPATH awareness */

static const char *not_found = "Message not found!!\n";
static struct db_info *db_info _INITBSS;
static int db_count _INITBSS, maxdb _INITBSS;

struct db_info {
	char	db_name[DB_NAME_LEN];	/* Name of the message file */
	caddr_t	addr;			/* Virtual memory address   */
	size_t	length;
	char	saved_locale[LC_NAMELEN];
        char	already_searched;	/* Already searched for that catalog,
					   none found */
};

/* Minimum number of open catalogues */
#define MINDB			3

static char cur_cat[DB_NAME_LEN] _INITBSSS;

#define set_path(x)     { if( ptr2 >= p_path_max ) { \
                            if (nlspath == def_nlspath)   \
                              return 0;                   \
                            nlspath = def_nlspath;        \
                            goto retry_with_def_nlspath;  \
                          } else                          \
                            *ptr2++ = (x);                \
                        }

static char *
cat_name(char *name, char *path, int len, struct stat * sbuf )
{
  const char *nlspath;
  const char *lang, *ptr, *ptr3;
  char *ptr2, c, d;
  const char *def_nlspath;
  char *p_path_max = path + len -1;

  /*
   * A name that contains a slash is a pathname
   */
  if (strchr(name, '/') != 0)
  {
      if ( stat ( name, sbuf ) )
	  return NULL;
      return name;
  }

  /* Allowed because we are protected by LOCKLOCALE */
  lang = _cur_locale [ LC_MESSAGES ];

  if ( !strcmp ( lang, "C" ) )
      def_nlspath = _C_LOCALE_DEF_NLSPATH;
  else
      def_nlspath = DEF_NLSPATH;

  /*
   * Get env variables
   */
  if ((nlspath = getenv(NL_PATH)) == 0){
    /*
     * No env var : use default
     */
    nlspath = def_nlspath;
  }

  if (!lang) {
        lang = getenv(NL_LANG);
        if (!lang) lang = NL_DEF_LANG;
  }

  /*
   * Scan nlspath and replace if needed
   */
retry_with_def_nlspath:
  ptr = nlspath;
  ptr2 = path;
  *ptr2 = '\0';
  for (;;){
    switch (c = *ptr++){
    case ':':
      if (ptr2 == path){
        /*
         * Leading ':' Check current directory
         */
	if ( stat ( name, sbuf ) == 0 )
          return name;
        continue;
      }
    /*FALLTHROUGH*/
    case '\0':
        /*
         * Found a complete path
         */
        *ptr2 = '\0';
        /*
         * Test to see that path is not a pure directory,
         * if it is then attempt toopen it and append the
         * filename "name" to it, in an attempt to succeed
         * This syntax is not explicitly defined in XPG2, but
         * is a logical extension to it. (XVS tests it too).
         */

        if ( stat(path, sbuf) )
	    goto nextpath;
        if ( S_ISDIR (sbuf -> st_mode) ) {
                (void) strcat(path, "/");
                if ( stat ( strcat(path, name), sbuf ) == 0)
                        return path;
                } /* otherwise branch out to end */
        else
	    return path;

nextpath:
        if (c == '\0'){
                /*
                 * File not found
                 */
                if (nlspath == def_nlspath)
                    return 0;
                nlspath = def_nlspath;
                ptr = nlspath;
        }
        ptr2 = path;
        *ptr2 = '\0';
        continue;
    case '%':
      /*
       * Expecting var expansion
       */
      switch(c = *ptr++){
      case 'L':
        ptr3 = lang;
        while ((d = *ptr3++) != 0)
	  set_path(d);
        continue;
      case 'l':
        ptr3 = lang;
        while ((d = *ptr3++) != 0 && d != '.' && d != '_')
	  set_path(d);
        continue;
      case 't':
        ptr3 = strchr(lang, '_');
        if (ptr3++ != 0){
          while ((d = *ptr3++) != 0 && d != '.')
	    set_path(d);
        }
        continue;
      case 'c':
        ptr3 = strchr(lang, '.');
        if (ptr3++ != 0){
          while ((d = *ptr3++) != 0)
	    set_path(d);
        }
        continue;
      case 'N':
        ptr3 = name;
        while ((d = *ptr3++) != 0)
	  set_path(d);
        continue;
      case '%':
	set_path('%');
        continue;
      default:
	set_path('%');
	set_path(c);
        continue;
      }
    default:
      set_path(c);
      continue;
    }
  }
}


/* setcat(cat): Specify the default catalogue.
 * Return a pointer to the local copy of the default catalogue
 */
const char *
setcat(const char *cat)
{
	LOCKDECLINIT(l, LOCKLOCALE);
	if (cat){
		(void) strncpy(cur_cat, cat, sizeof cur_cat - 1);
		cur_cat[sizeof cur_cat - 1] = '\0';
	}
	UNLOCKLOCALE(l);
	return cur_cat[0] ? cur_cat : NULL;
}

/* __gtxt(catname, id, dflt): Return a pointer to a message.
 *	catname is the name of the catalog. If null, the default catalog is
 *		used.
 *	id is the numeric id of the message in the catalogue
 *	dflt is the default message.
 *	
 *	Information about non-existent catalogues is kept in db_info, in
 *	such a way that subsequent calls with the same catalogue do not
 *	try to open the catalogue again.
 */
const char *
__gtxt(const char *catname, int id, const char *dflt)
{
	char pathname[PATH_MAX];
	register int i;
	struct db_info *db;
	LOCKDECL ( l );

	/* Check for invalid message id */
	if (id < 0)
		return not_found;

	LOCKINIT(l, LOCKLOCALE);

	/* First time called, allocate space */
	if (!db_info){
		if ((db_info = (struct db_info *)
			malloc(MINDB * sizeof(struct db_info))) == 0)
			goto out_of_memory_err;
		maxdb = MINDB;
	}

	/* If catalogue is unspecified, use default catalogue.
	   No catalogue at all is an error */
	if (!catname || !*catname){
		if (!cur_cat || !*cur_cat)
			goto err;
		catname = cur_cat;
	}

	/* Retrieve catalogue in the table */
	for (i = 0 ; i < db_count ; i++){
	    if (strcmp(catname, db_info[i].db_name) == 0)
		break;
	}
	/* New catalogue */
	if (i == db_count){
	    if (db_count == maxdb){
		if ((db_info = (struct db_info *)
		     realloc(db_info, (size_t)(++maxdb) * sizeof(struct db_info))) == 0)
		    goto out_of_memory_err;
	    }
	    strcpy ( db_info[i].db_name, catname );
	    db_info[i].addr = 0;
	    db_info[i].already_searched = 0;
	    db_count++;
	}
	else
	{
	    /* Check for a change in locale. If necessary unmap the
	     * catalogue.  The entry in the table is NOT freed. The
	     * catalogue offset remains valid. */
	    if (strcmp(_cur_locale[LC_MESSAGES], db_info[i].saved_locale) != 0)
	    {
		if ( db_info[i].addr )
		    munmap(db_info[i].addr, db_info[i].length);
		db_info[i].addr = 0;
		db_info[i].already_searched = 0;
	    }
	}
	db = &db_info[i];

	/* Retrieve the message from the catalogue */

	/* Open and map catalogue if not done yet. In case of
	 * failure, mark the catalogue as non-existent */
	if ( ! db->already_searched && db->addr == 0 )
	{
	    int		fd = -1;
	    void *	addr;
	    struct stat	sb;
	    int		save_errno = oserror();

	    db->already_searched = 1;
	    strcpy ( db_info[i].saved_locale, _cur_locale [ LC_MESSAGES ] );

	    if (! cat_name (db->db_name, pathname, PATH_MAX, & sb)
		|| (fd = open(pathname, O_RDONLY)) == -1 
		|| (addr = mmap(0, (size_t)sb.st_size, 
				PROT_READ, MAP_SHARED,
				fd, 0)) == MAP_FAILED){

		/* Something has failed */
		if (fd != -1)
		    close(fd);
	    }
	    else
	    {
		close ( fd );
		db->addr = addr;
		db->length = (size_t)sb.st_size;
	    }
	    setoserror(save_errno);

	}

	/* Return the message from the catalogue */
	if ( db->addr && id != 0 && id <= *(int *)(db->addr)){
	    UNLOCKLOCALE(l);
	    return (char *)(db->addr + *(int *)(db->addr + 
						id * sizeof(int)));
	}
	else
	{
	    UNLOCKLOCALE(l);
	    return (dflt && *dflt) ? dflt : not_found;
	}

	
out_of_memory_err:
	UNLOCKLOCALE(l);
	return (sys_errlist[oserror()]);
err:
	UNLOCKLOCALE(l);
	return not_found;
}
