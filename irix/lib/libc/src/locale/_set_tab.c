/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/_set_tab.c	1.8"
/*
 * _set_tab - set _numeric[] and ctype-related members of __libc_attr 
 * structure to requested locale information.
 */
#include "synonyms.h"
#include <locale.h>
#include "_locale.h"
#include "_wchar.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <iconv_cnv.h>
#include <iconv_int.h>
#include "mplib.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/euc.h>

static void set_locale_attr ( const char * loc );

#define OFFSET(s,m)	( (size_t) &(((s *) NULL )->m) )
#define OFFSETA(s,a)	( (size_t)  (((s *) NULL )->a) )
#define SIZEOF(s,m)	( sizeof ( ( ( s * ) NULL ) -> m ) )

#define CTYPE_MAGIC_NB	0x77229966
typedef struct {
    int			magic_number;

    unsigned char	ctype [ 257 ];
    unsigned char	uplo [ 257 ];
    char		cswidth [ 7 ];
    char		fill [ 3 ];
} old_ctype_t ;

#define SZ_TOTAL_NEW	( sizeof ( old_ctype_t ) + sizeof ( __ctype_t ) )

typedef struct mm_list_t {
    struct mm_list_t *	next;
    void *	map;
    char *	name;
    size_t	len;
    ino_t	inode;
} mm_list_t;


static mm_list_t * mm_head = NULL;

int
_set_tab_numeric ( const char * loc )
{
    register int fd;
    unsigned char my_numeric[ SZ_NUMERIC ];

    assert(ISLOCKLOCALE);

    if ((fd = open(_fullocale(loc, "LC_NUMERIC"), O_RDONLY)) == -1)
	return -1;
	    
    if ( read(fd, (char *)my_numeric, SZ_NUMERIC ) == (size_t) SZ_NUMERIC )
    {
	memcpy(_numeric, my_numeric, SZ_NUMERIC );
	close (fd);
	return 0;
    }
    close (fd);
    return -1;
}

int
_set_tab_ctype ( const char * loc )
{
	register void * map = NULL;
	register int	i;
	char *		name;
	int		save_errno = oserror(); /* save the errno */
	mm_list_t *	p = NULL;

	assert(ISLOCKLOCALE);

	/* the __mbwc_setmbstate() set the errno if it can not */
	/* open the mbstate_t successfully. 	           */
	/* The errno is required by the new spec, however,     */
	/* the setlocale() spec stated no errno required.      */
	/* therefore, we will reset the errno back */
	if ( __mbwc_setmbstate( loc ) == -1) {
	    /* reset it back since setlocale() did not ask for it */
	cleanup:
	    setoserror(save_errno);
	    return -1;
	}

	set_locale_attr ( loc );

	name = _fullocale ( loc, "LC_CTYPE" );

	/* Search if the file has already been mapped */
	for ( p = mm_head; p ; p = p->next )
	    if ( ! strcmp ( p->name , name ) )
		break;

	if ( ! p )
	{
	    /* No map found. Let's search based on the inode */
	    struct stat f_stat;
	    mm_list_t * n;

	    /* Check the size of the file */
	    if ( stat ( name, & f_stat ) == -1 
		 || f_stat.st_size < SZ_TOTAL_NEW )
		goto cleanup;

	    /* We will in any case need a new entry in the list */
	    /* Note that name is appended at the end of the node */
	    n = (mm_list_t *) malloc ( sizeof (mm_list_t) + strlen ( name ) + 1);
	    n->next = mm_head;
	    n->name = strcpy ( (char*)n + sizeof (mm_list_t), name );
	    n->len = f_stat.st_size;
	    n->inode = f_stat.st_ino;

	    /* Now let's look for an entry with same inode */
	    for ( p = mm_head; p ; p = p->next )
		if ( f_stat.st_ino == p->inode )
		{
		    /* Found it */
		    n->map = p->map;
		    break;
		}
	    if ( ! p )
	    {
		/* Still not found. Need to be mapped in */
		register int fd;
		
		/* Open the file */
		if ( ( fd = open( name, O_RDONLY ) ) == -1 )
		{
		    free ( n );
		    goto cleanup;
		}
		/* Map the file in */
		if ( ( map = mmap ( 0, f_stat.st_size , PROT_READ, MAP_SHARED, fd, 0 ) )
		     == MAP_FAILED )
		{
		    free ( n );
		    close ( fd );
		    goto cleanup;
		}

		close ( fd );

		/* Check for correct magic number */
		if ( *(int *) map != CTYPE_MAGIC_NB )
		    goto cleanup;
		
		n->map = map;	/* Keep track of the newly mmap area */

	    }

	    mm_head = n;	/* Add the new node to top of list */
	    p = n;		/* Use the newly created node */
	}

	/* We now have a valid mmap area */
	map = p->map;

	/* Place old ctype table */
	memcpy ( _ctype, (void *) ((addr_t)map + SIZEOF ( old_ctype_t, magic_number ) ), 
		 sizeof ( old_ctype_t ) 
		 - SIZEOF ( old_ctype_t, fill )
		 - SIZEOF ( old_ctype_t, magic_number ) );
	
	/* Point _ctype_tbl to new table */
	__libc_attr._ctype_tbl = (__ctype_t *) ( (addr_t)map + sizeof ( old_ctype_t ) );
	
	/* Initialize cswidth */
	memcpy( & __libc_attr._csinfo,
		(void *) ( (addr_t) map + sizeof ( old_ctype_t ) + OFFSETA(__ctype_t, _cswidth) ),
		SIZEOF ( __ctype_t, _cswidth ) );
	
	/* Is there a wchrtbl */
	if ( p->len > SZ_TOTAL_NEW )
	{
	    map = (void*) ( (addr_t)map + SZ_TOTAL_NEW );
	    
	    /* Initialize the _wcptr tables */
	    for (i=0; i<3; i++)
	    {
		_wcptr[i] = (struct _wctype *)((addr_t)map + ((sizeof(struct _wctype)) * i));
		if ( _wcptr[i]->index_off != 0)
		    _wctbl[i].index = (unsigned char *)((addr_t)map + _wcptr[i]->index_off );
		else
		    _wctbl[i].index = 0;
		
		if (_wcptr[i]->type_off != 0)
		    _wctbl[i].type = (unsigned int *)((addr_t)map + _wcptr[i]->type_off);
		else
		    _wctbl[i].type = 0;
		
		if (_wcptr[i]->code_off != 0)
		    _wctbl[i].code = (wchar_t *)((addr_t)map + _wcptr[i]->code_off);
		else
		    _wctbl[i].code = 0;
	    }
	    
	}
	else
	    for ( i=0; i<3; i++ )
	    {
		_wcptr[i] = 0;
		_wctbl[i].index = 0;
		_wctbl[i].type = 0;
		_wctbl[i].code = 0;
	    }

	return 0;
}

#define EUC_FUNC_PREFIX	"euc-"
typedef struct {
    int		magic_no;
    int		is_euc;
    ICONV_REFER	isset2[1];
    ICONV_REFER	isset3[1];
    ICONV_REFER	iscodeset[1];
} euc_func_t;

void
set_locale_attr ( const char * loc )
{
    char resource[256];
    euc_func_t	* euc_func;
    void        * ref;

    /* euc dependent functions */
    strcpy ( resource, EUC_FUNC_PREFIX );
    strncat ( resource, loc, sizeof(resource) - sizeof ( EUC_FUNC_PREFIX ) );

    if (
	( euc_func = __iconv_get_resource ( resource, NULL ) ) &&
	euc_func->magic_no == ICONV_MAGIC_RESVALS
	)
    {
	__libc_attr._euc_func._is_euc = euc_func -> is_euc;

	/* Warning: iconv_open_reference can return both 0 and -1 to indicate error */

	ref = __iconv_open_reference ( euc_func -> isset2 );
	if ( ref == (void *)(ssize_t) -1 || ref == NULL )
	    __libc_attr._euc_func._isset2 = __isset2;
	else
	    __libc_attr._euc_func._isset2 = (__isset_func_t) ref;

	ref = __iconv_open_reference ( euc_func -> isset3 );
	if ( ref == (void *)(ssize_t) -1 || ref == NULL )
	    __libc_attr._euc_func._isset3 = __isset3;
	else
	    __libc_attr._euc_func._isset3 = (__isset_func_t) ref;

	ref = __iconv_open_reference ( euc_func -> iscodeset );
	if ( ref == (void *)(ssize_t) -1 || ref == NULL )
	    __libc_attr._euc_func._iscodeset = __iscodeset;
	else
	    __libc_attr._euc_func._iscodeset = (__iscodeset_func_t) ref;

    }
    else
    {
	__libc_attr._euc_func._is_euc = 1;
	__libc_attr._euc_func._isset2 = __isset2;
	__libc_attr._euc_func._isset3 = __isset3;
	__libc_attr._euc_func._iscodeset = __iscodeset;
    }
}

#define COLLATE_RES_PREFIX	"collate-"
#define RES_PREFIX_MAX_LEN	32

typedef struct {
    int			magic_no;
    unsigned char	coll_as_cmp;
} _collate_res_t;

void
_set_res_collate ( const char * loc )
{
    char		resource[ LC_NAMELEN + RES_PREFIX_MAX_LEN ] = COLLATE_RES_PREFIX;
    _collate_res_t *	res;

    strncat ( resource, loc, sizeof(resource) - sizeof ( COLLATE_RES_PREFIX ) );

    if (
	( res = __iconv_get_resource ( resource, NULL ) ) &&
	res->magic_no == ICONV_MAGIC_RESVALS
	)
	__libc_attr._collate_res._coll_as_cmp = res->coll_as_cmp;
    else
	__libc_attr._collate_res._coll_as_cmp = 0;

}
