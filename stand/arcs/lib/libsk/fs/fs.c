#ident	"lib/libsk/fs/fs.c:  $Revision: 1.17 $"

/*
 * fs.c - common filesystem utilities
 */

#include "saio.h"
#include <arcs/folder.h>
#include <arcs/fs.h>
#include <arcs/eiob.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>

static FS_ENTRY *fs_list;		/* list of registered filesystems */

/*
 * fs_init - initialize filesys data structures from table in conf.c
 */
void
fs_init(void)
{
    extern int (*_fs_table[])(void);
    int i;

    fs_list = (FS_ENTRY *) 0;

    /* search the filesys initialization array and call each
     * install function found there
     */
    for (i = 0; _fs_table[i]; i++)
	(*_fs_table[i])();
}

int
fs_register (int (*func)(), char *name, int type)
{
    FS_ENTRY *fs_new;

#ifdef	DEBUG
    printf ("Registering filesystem \"%s\", type %d\n", name, type);
#endif

    if ((fs_new = (FS_ENTRY *)malloc(sizeof(FS_ENTRY))) == 0) {
#ifdef	DEBUG
	printf ("Could not register \"%s\", out of memory.\n", name);
#endif
	return ENOMEM;
    }

    if (strlen(name) > FS_NAME_MAX_LEN) {
#ifdef	DEBUG
	printf ("Could not register \"%s\", name too long.\n", name);
#endif
	free (fs_new);
	return ENAMETOOLONG;
    }

    /* inserts new filesys at head of list
     */
    strcpy (fs_new->Identifier, name);
    fs_new->FSStrategy = func;
    fs_new->Type = type;
    fs_new->Next = fs_list;
    fs_list = fs_new;

    return ESUCCESS;
}

int
fs_unregister (char *name)
{
    FS_ENTRY *fs_cur, *fs_prev;

#ifdef	DEBUG
    printf ("Unregistering filesystem \"%s\"\n", name);
#endif
    for (fs_prev = fs_cur = fs_list; fs_cur != 0;
	    fs_prev = fs_cur, fs_cur = fs_cur->Next) {
	if (!strcmp(fs_cur->Identifier, name))
	    break;
    }

    if (!fs_cur) {
#ifdef	DEBUG
	printf ("Filesystem \"%s\" not found.\n", name);
#endif
	return EINVAL;
    }

    if (fs_cur == fs_list)
	fs_list = fs_cur->Next;
    else
	fs_prev->Next = fs_cur->Next;
    free (fs_cur);

    return ESUCCESS;
}

/*
 * fs_search - search all of the current filesystems to see if 
 * the io file/device is valid
 */
int
fs_search (struct eiob *io)
{
    FS_ENTRY *entry = fs_list;
    unsigned int type;

    io->fsstrat = 0;
    io->fsb.FunctionCode = FS_CHECK;

    /* Create a bitmap of the allowable filesystem types.
     * If it's a network device, search only network filesystems,
     * if its a disk search only disk filesystems, etc.
     */
    switch (io->dev->Type) {
    case NetworkPeripheral:
    case NetworkController:
	type = DTFS_BOOTP | DTFS_RIPL;
	break;
    case CDROMController:
    case WORMController:
    case DiskController:
    case DiskPeripheral:
	type = DTFS_EFS | DTFS_XFS | DTFS_FAT | DTFS_DVH;
	break;
    case FloppyDiskPeripheral:
	type = DTFS_FAT | DTFS_TAR;
	break;
    case TapeController:
    case TapePeripheral:
	type = DTFS_TAR | DTFS_TPD;
	break;
    default:
	return EINVAL;
    }

    while (entry) {
	if (entry->Type & type) {
	    if ((*entry->FSStrategy)(&io->fsb) == ESUCCESS) {
		    io->fsstrat = entry->FSStrategy;
		    return ESUCCESS;
	    }
	}
	entry = entry->Next;
    }

    return EINVAL;
}

/*  Function used by parse_path() to match protocol against currently
 * installed filesystems based on names given to fs_register().  Return
 * the DTFS_* type on match, or 0 on a non-match.
 */
int (*fs_match(char *name, int len))()
{
	FS_ENTRY *fs;

	for (fs = fs_list; fs ; fs = fs->Next) {
		if ((strncmp(name,fs->Identifier,len) == 0) &&
		    (len == strlen(fs->Identifier)))
			return(fs->FSStrategy);		/* found */
	}
	return(0);					/* failure */
}
