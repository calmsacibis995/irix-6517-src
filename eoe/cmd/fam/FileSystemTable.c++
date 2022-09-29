#include "FileSystemTable.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

#include "Cred.h"
#include "Event.h"
#include "FileSystem.h"
#include "InternalClient.h"
#include "LocalFileSystem.h"
#include "Log.h"
#include "NFSFileSystem.h"

//  Fam has two tables of mounted filesystems -- fs_by_name and
//  fs_by_id.  They are keyed by mountpoint and by filesystem ID,
//  respectively.  fs_by_id is lazily filled in as needed (so we only
//  do as many statvfs calls as needed -- fam may hang if the NFS
//  server is down).a fs_by_name is completely rebuilt when /etc/mtab
//  is changed, and fs_by_id is destroyed, to be lazily re-filled
//  later.

//  Class Variables

unsigned int		    FileSystemTable::count;
FileSystemTable::IDTable    FileSystemTable::fs_by_id;
FileSystemTable::NameTable *FileSystemTable::fs_by_name;
const char		    FileSystemTable::mtab_name[] = MOUNTED;
InternalClient		   *FileSystemTable::mtab_watcher;
FileSystem		   *FileSystemTable::root;

#ifdef NO_LEAKS

//////////////////////////////////////////////////////////////////////////////
//  The constructor and destructor simply maintain a refcount of the
//  files that include FileSystemTable.H.  When the last reference
//  is destroyed, the mtab watcher is turned off, and the fs_by_name
//  table is destroyed.

FileSystemTable::FileSystemTable()
{
    count++;
}

FileSystemTable::~FileSystemTable()
{
    if (!--count)
    {   delete mtab_watcher;
	mtab_watcher = NULL;
	if (fs_by_name)
	{   destroy_fses(fs_by_name);
	    delete fs_by_name;
	    fs_by_name = NULL;
	}
    }
}

#endif /* NO_LEAKS */

//////////////////////////////////////////////////////////////////////////////
//  fs_by_name is a table that maps mount directories to FileSystem pointers.
//
//  It is built the first time FileSystemTable::find() is called, and
//  it's rebuilt whenever /etc/mtab changes.  When it is rebuilt,
//  existing FileSystems are moved to the new table.  This is done
//  because each ClientInterest has a pointer to its FileSystem,
//  and we don't want to change all ClientInterests, nor do we want
//  two FileSystem structures representing the same file system.


void
FileSystemTable::create_fs_by_name()
{
    NameTable *new_fs_by_name = new NameTable;
    NameTable mount_parents, dismounted_fses;

    if (fs_by_name)
	dismounted_fses = *fs_by_name;

    //  Read /etc/mtab.  (Open for update to lock file.)

    Cred::SuperUser.become_user();
    FILE *mtab = setmntent(mtab_name, "r+");
    root = NULL;
    for (mntent *mp; mp = getmntent(mtab); )
    {
	FileSystem *fs = fs_by_name ? fs_by_name->find(mp->mnt_dir) : NULL;
	if (fs && fs->matches(*mp))
	{
	    Log::debug("mtab: MATCH     \"%s\" on \"%s\" using type <%s>",
		       mp->mnt_fsname, mp->mnt_dir, mp->mnt_type);

	    new_fs_by_name->insert(mp->mnt_dir, fs);
	    if (dismounted_fses.find(mp->mnt_dir))
		dismounted_fses.remove(mp->mnt_dir);
	}
	else
	/* This broke when NFS 3 was introduced, so it was changed ... ECZ
	{   if (!strcmp(mp->mnt_type, FSID_NFS) ||
	******/
	{   if ((!strcmp(mp->mnt_type, FSID_NFS) ||
		!strcmp(mp->mnt_type, FSID_NFS2)||
		!strcmp(mp->mnt_type, FSID_NFS3)||
		!strcmp(mp->mnt_type, FSID_CACHEFS)) &&
		strchr(mp->mnt_fsname, ':'))
	    {
		Log::debug("mtab: new NFS   \"%s\" on \"%s\" %s using <%s>",
			   mp->mnt_fsname, mp->mnt_dir, hasmntopt(mp, "dev"),
				mp->mnt_type);

		fs = new NFSFileSystem(*mp);
	    }
	    else
	    {
		Log::debug("mtab: new local \"%s\" on \"%s\"",
			   mp->mnt_fsname, mp->mnt_dir);

		fs = new LocalFileSystem(*mp);
	    }
	    new_fs_by_name->insert(mp->mnt_dir, fs);
	    if (fs_by_name)
	    {
		// Find parent filesystem.

		FileSystem *parent = longest_prefix(mp->mnt_dir);
		assert(parent);
		mount_parents.insert(parent->dir(), parent);
	    }
	}
	if (!strcmp(mp->mnt_dir, "/"))
	    root = fs;
    }
    assert(root);
    endmntent(mtab);

    //  Install the new table.

    delete fs_by_name;
    fs_by_name = new_fs_by_name;

    //  Relocate all interests in parents of new filesystems.
    //  We relocate interests out of parents before relocating
    //  out of dismounted filesystems in the hope that we can
    //	avoid relocating some interests twice.  Consider the case
    //  where /mnt/foo is an interest, and we simultaneously
    //  learn that /mnt was dismounted and /fred was mounted.
    //  We don't want to relocate /mnt/foo to /, then test
    //  it for relocation to /fred.

    unsigned i;
    FileSystem *fs;
    for (i = 0; fs = mount_parents.value(i); i++)
    {
	Log::debug("mtab: relocating in parent \"%s\"", fs->dir());
	fs->relocate_interests();
    }

    //  Relocate all interests in dismounted filesystems and destroy
    //  the filesystems.

    for (i = 0; fs = dismounted_fses.value(i); i++)
    {
	Log::debug("mtab: dismount  \"%s\" on \"%s\"",
		   fs->fsname(), fs->dir());

	fs->relocate_interests();
	delete fs;
    }
    Log::debug("mtab done.");
}

void
FileSystemTable::destroy_fses(NameTable *fstab)
{
    //  Destroy any unreclaimed filesystems.

    for (unsigned i = 0; fstab->key(i); i++)
	delete fstab->value(i);
}

void
FileSystemTable::mtab_event_handler(const Event& event, void *)
{
    if (event == Event::Changed)
    {
	Log::debug("%s changed, rebuilding filesystem table", mtab_name);
	for (ulong_t fsid; fsid = fs_by_id.first(); )
	    fs_by_id.remove(fsid);
	create_fs_by_name();
    }
}

//////////////////////////////////////////////////////////////////////////////
//

FileSystem *
FileSystemTable::find(const char *path, const Cred& cr)
{
    //  (Initialize fs_by_name if necessary.)  As a side effect,
    //  create_fs_by_name initializes our "root" member variable.

    if (!fs_by_name)
    {   create_fs_by_name();
	mtab_watcher = new InternalClient(mtab_name, mtab_event_handler, NULL);
    }

//  Perform a statvfs(2) on the first existing ancestor.

    struct statvfs fs_status;
    char temp_path[MAXPATHLEN];

    cr.become_user();
    assert(path[0] == '/');
    int rc = statvfs(path, &fs_status);
    if (rc < 0)
    {   (void) strcpy(temp_path, path);
	while (rc < 0)
	{   char *slash = strrchr(temp_path, '/');
	    if (!slash)
		return root;
	    *slash = '\0';
	    rc = statvfs(temp_path, &fs_status);
	}
    }

    //  Look up filesystem by ID.

    FileSystem *fs = fs_by_id.find(fs_status.f_fsid);
    if (fs)
	return fs;

    //  Convert to real path.  Look up real path in fs_by_name().

    cr.become_user();
    (void) realpath(path, temp_path);
    fs = longest_prefix(temp_path);

    //  Insert into fs_by_id.

    fs_by_id.insert(fs_status.f_fsid, fs);
    return fs;
}

FileSystem *
FileSystemTable::longest_prefix(const char *path)
{
    FileSystem * bestmatch = root;
    int maxmatch = -1;
    const char *key;
    for (unsigned i = 0; key = fs_by_name->key(i); i++)
    {   for (int j = 0; ; j++)
	{   if (!key[j])
	    {   if ((!path[j] || path[j] == '/') && j > maxmatch)
		{   maxmatch = j;
		    bestmatch = fs_by_name->value(i);
		}
		break;
	    }
	    if (key[j] != path[j])
		break;
	}
    }
    assert(bestmatch);
    return bestmatch;
}
