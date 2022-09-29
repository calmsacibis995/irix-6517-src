#include "Volume.H"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <mediaclient.h>
#include <mntent.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CallBack.H"
#include "Config.H"
#include "Device.H"
#include "DSReq.H"
#include "Event.H"
#include "Log.H"
#include "Partition.H"
#include "Task.H"
#include "strplus.H"

//  mount_task is a schedulable task.  All mounting is done from
//  the mount task.

Task Volume::mount_task(mount_proc, NULL);

//  Volumes can be enumerated.  Volume::volumes keeps track of them.

Enumerable::Set Volume::volumes;

//  The Volume constructor initializes a truckload of variables.

// XXX Security fix late in IRIX 6.5 release.
const char * Volume::_globalOptions = NULL;

Volume::Volume(const VolumeAddress& va, const mntent& mnt, const char *label)
: Enumerable(volumes),
  _address(va),
  _label(NULL),
  _fsname(NULL),
  _type(NULL),
  _state(VS_NOT_MOUNTED),
  _is_ignored(false),
  _is_shared(false),
  _needs_mount(false),
  _forced_unmount(false),
  _default_dir(NULL),
  _config_dir(NULL),
  _sub_dir(NULL),
  _default_opts(NULL),
  _config_opts(NULL),
  _all_opts(NULL),
  _subformat(NULL),
  _config(new Config(config_proc, this))
{
    if (label)
	_label = strdupplus(label);

    assert(mnt.mnt_fsname != NULL);
    _fsname = strdupplus(mnt.mnt_fsname);

    assert(mnt.mnt_dir != NULL);
    _default_dir = strdupplus(mnt.mnt_dir);
    
    assert(mnt.mnt_type != NULL);
    _type = strdupplus(mnt.mnt_type);

    if (mnt.mnt_opts != NULL)
	_default_opts = strdupplus(mnt.mnt_opts);

    //	Get the config options from the config file.  The call to
    //	set_ignored() may automatically schedule the Volume for
    //	mounting, but the Volume will not be mounted until the mount
    //	task runs.

    set_config_dir(_config->mount_dir(va));
    set_config_options(_config->mount_options(va));
    set_ignored(_config->volume_is_ignored(va));
//    set_shared(is_shared_heuristic());
}

//  The Volume destructor deallocates a truckload of strings.

Volume::~Volume()
{
    delete _config;
    delete [] _all_opts;
    delete [] _config_opts;
    delete [] _default_opts;
    delete [] _sub_dir;
    delete [] _config_dir;
    delete [] _default_dir;
    delete [] _type;
    delete [] _fsname;
    delete [] _label;
}

//  set_ignored sets whether this Volume is ignored.  If
//  the volume is ignored, it's dismounted, otherwise
//  it's mounted.

void
Volume::set_ignored(bool value)
{
    _is_ignored = value;
    if (!_is_ignored && _state != VS_MOUNTED)
    {	_needs_mount = true;
	mount_task.schedule(Task::ASAP);
    }
    else if (_is_ignored && _state == VS_MOUNTED)
	dismount(false);
}

//  The heuristic for sharing is: if the volume is explicitly
//  shared or hidden, share/hide it.  Otherwise, if all partitions'
//  devices are shared, share it.  Otherwise, hide it.

bool
Volume::is_shared_heuristic() const
{
    if (_config->mount_is_shared(address()))
	return true;
    if (_config->mount_is_unshared(address()))
	return false;

    // Check all devices.  If any is not shared, do not export.

    for (unsigned int i = 0; i < npartitions(); i++)
	if (!_config->device_is_shared(partition(i)->device()->address()))
	    return false;

    return true;
}

void
Volume::set_shared(bool value)
{
    Event::Type t = Event::None;
    bool _was_shared = _is_shared;
    if (_was_shared && !value)
    {   t = Event::Unexport;
	if (_state == VS_MOUNTED)
	    unshare();
    }
    _is_shared = value;
    if (!_was_shared && _is_shared)
    {   t = Event::Export;
	if (_state == VS_MOUNTED)
	share();
    }

    // Generate an event for those lucky clients.

    if (t != Event::None)
	CallBack::activate(Event(t, NULL, this));
}

//  Return the one true directory where this volume should
//  be mounted.  The heuristic is described in Volume.H.

const char *
Volume::dir() const
{
    if (_config_dir)
	return _config_dir;
    else if (_sub_dir)
	return _sub_dir;
    else
	return _default_dir;
}

//  Set this volume's config directory.  If the config directory
//  has changed, force the volume to remount.

void
Volume::set_config_dir(const char *new_config_dir)
{
    if (strcmpnull(_config_dir, new_config_dir) != 0)
    {
	if (is_mounted())
	    dismount(false);
	delete [] _config_dir;
	if (new_config_dir)
	    _config_dir = strdupplus(new_config_dir);
	else
	    _config_dir = NULL;
    }
}

//  Set this Volume's subdirectory.  Assumes that the Volume
//  isn't mounted. (I.e., the subdirectory is only set once
//  shortly after volume creation.)  This method is called
//  from a derived class's use_subdir() function.

void
Volume::set_sub_dir(const char *new_subdir)
{
    assert(_state != VS_MOUNTED);
    if (strcmpnull(_sub_dir, new_subdir) != 0)
    {
	delete [] _sub_dir;
	if (new_subdir)
 	    _sub_dir = strdupplus(new_subdir);
	else
	    _sub_dir = NULL;
    }
}

//  Calculate whether this Volume needs a subdirectory, and
//  construct it if needed.  The actual construction is handed
//  to the derived class's use_subdir() method.

void
Volume::calc_sub_dir()
{
    assert(_state != VS_MOUNTED);
    if (!_sub_dir && dir_collides())
	use_subdir();
    else
	set_sub_dir(NULL);
}

//  Test whether this Volume has the same base directory as another
//  Volume.

bool
Volume::dir_collides()
{
    const char *my_dir = base_dir();
    assert(my_dir != NULL);
    for (Volume *that = first(); that; that = that->next())
    {   assert(that->base_dir() != NULL);
	if (that != this && !strcmp(my_dir, that->base_dir()))
	    return true;
    }
    return false;
}

//  Set the configuration options.  If they've changed, than dismount
//  the volume and delete _all_opts so that _all_opts will be
//  recomputed before remounting.

void
Volume::set_config_options(const char *new_config_opts)
{
    if (strcmpnull(_config_opts, new_config_opts) != NULL)
    {
	if (is_mounted())
	    dismount(false);
	delete [] _config_opts;
	if (new_config_opts)
	    _config_opts = strdupplus(new_config_opts);
	else
	    _config_opts = NULL;

	//  Zap _all_opts so _all_opts will be recomputed next time
	//  it's used.

	delete [] _all_opts;
	_all_opts = NULL;
    }
}

//  Volume::options() returns the mount options string.  It lazily
//  builds it as needed.  The only option that gets special handling
//  is the rwopt (read-only vs. read-write).  The options string is the
//  concatenation of rwopt, the config options, and the default options,
//  with duplicates removed.

const char *
Volume::options()
{
    if (_all_opts)
	return _all_opts;

    //  Resolve read write vs. read only.
    //
    //	If default_opts includes "ro", then volume is ro.
    //	If config_opts includes "ro", then volume is ro.
    //	If any device is read-only or any medium is write-protected,
    //	then volume is ro.
    //	Otherwise, volume is rw.

    static const char rw[] = "rw";
    static const char ro[] = "ro";
    const char *rwopt;
    mntent mnt;
    if ((mnt.mnt_opts = _default_opts) && hasmntopt(&mnt, ro))
	rwopt = ro;
    else if ((mnt.mnt_opts = _config_opts) && hasmntopt(&mnt, ro))
	rwopt = ro;
    else
    {
	rwopt = rw;
	const Partition *p;
	for (unsigned i = 0; p = partition(i); i++)
	{   Device *device = p->device();
	    if (device->feature_rdonly() || device->is_write_protected())
	    {   rwopt = ro;
		break;
	    }
	}
    }

    //  Now glom all options into one string: the rwopt,
    //	the config options and the default options.
    //	We don't try to resolve conflicting options here
    //	except for identical options and rw vs. ro.

    char options[100];
    strcpy(options, rwopt);
    if (_config_opts)
    {
	char tempbuf[256];
	strcpy(tempbuf, _config_opts);
	for (char *p = tempbuf, *opt; *(opt = mntopt(&p)); )
	{   if (!strcmp(opt, ro) || !strcmp(opt, rw))
		continue;		// ro, rw already handled.
	    //  Append option.
	    strcat(options, ",");
	    strcat(options, opt);
	}
    }

    if (_default_opts)
    {
	char tempbuf[256];
	strcpy(tempbuf, _default_opts);
	for (char *p = tempbuf, *opt; *(opt = mntopt(&p)); )
	{   if (!strcmp(opt, ro) || !strcmp(opt, rw))
		continue;		// ro, rw already handled.

	    // Extract the option's name.  If an option by the same
	    // name is already there, disregard this option.

	    char *eq = strchr(opt, '=');
	    if (eq)
		*eq = '\0';
	    mntent temp_mntent;
	    temp_mntent.mnt_opts = options;
	    if (hasmntopt(&temp_mntent, opt))
		continue;
	    if (eq)
		*eq = '=';

	    //  Append option.

	    strcat(options, ",");
	    strcat(options, opt);
	}
    }

    // XXX Security fix late in IRIX 6.5 release.
    if (_globalOptions != NULL) {
	strcat(options, ",");
	strcat(options, _globalOptions);
    }

    _all_opts = strdupplus(options);
    return _all_opts;
}

//  Utility function used by Volume::options().  Returns the
//  first comma-delimited substring of a string.  Modifies
//  the argument string in place and moves the pointer past
//  the comma.

char *
Volume::mntopt(char **p)
{
    char *cp = *p;
    char *retstr;

    while (*cp && isspace(*cp))
	cp++;
    retstr = cp;
    while (*cp && *cp != ',')
	cp++;
    if (*cp) {
	*cp = '\0';
	cp++;
    }
    *p = cp;
    return (retstr);
}

//  Volume::mount() mounts a Volume.  It also tries to export the
//  filesystem.

void
Volume::mount()
{
    assert(!_is_ignored);
    assert(_needs_mount);
    _needs_mount = false;

    //	Weirdly, asking for the options string can open the /dev/scsi
    //	device.  So we ask for the options here, then close all devices.

    (void) options();
    DSReq::close_all();

    //	Create the mount point, if necessary.

    if (make_dir(dir()) != 0)
	return;

    //	Build the mount command and run it.

    const char *mount_argv[8] = {
	"/etc/mount", "-t", type(), "-o", options(), fsname(), dir(), NULL
    };
    int mount_status = execute(mount_argv);

    if (mount_status != 0)
    {
	// On the off chance that the mount failed because the device
	// was already mounted, try to dismount it and retry the mount.
	const char *umount_argv[8] = { "/etc/umount", fsname(), NULL };
	execute(umount_argv);	// umount doesn't return non zero with
				// bad filesys so just have to try again.
	mount_status = execute(mount_argv);
	if (mount_status != 0)
	{
	    _state = VS_MOUNT_FAILED;
	    Log::critical("The file system on device: %s cannot be mounted",
		      fsname());
	    return;
	}
    }
    _state = VS_MOUNTED;

    //  Do the export fu.

    if (_is_shared)
	share();

    //  Look for inst subformat on this volume.

    find_inst_subformat();

    find_photoCD_subformat();

    //  Send an event to everybody who cares.

    CallBack::activate(Event(Event::Mount, NULL, this));
}

//  Volume::dismount() dismounts a filesystem.  Inverse of Volume::mount().
//  If the force argument is set, uses "umount -k".  This should only
//  be used if the disk has been hardware ejected.

void
Volume::dismount(bool force)
{
    assert(_state == VS_MOUNTED);

    //  Do the unexport fu.

    if (_is_shared)
	unshare();

    if (force)
    {
	// set _forced_unmount for use by our callback forced_unmount().
	_forced_unmount = true;
	CallBack::activate(Event(Event::ForceUnmount, NULL, this));
	_forced_unmount = false;
	const char *fuser_argv[8] = {
	    "/sbin/fuser", "-kc", dir(), NULL
	};
	if (execute(fuser_argv) != 0)
	{
	    Log::critical("Can't kill processes using device %s prior to dismount", fsname());
	    return;
	}
    }

    const char *umount_argv[8] = {
	"/etc/umount", dir(), NULL
    };
    if (execute(umount_argv) != 0)
    {
	Log::critical("The file system on device: %s cannot be dismounted",
		      fsname());
	return;
    }
    _state = VS_NOT_MOUNTED;

    //	Remove the subdirectory, if any.

    if (_sub_dir)
	remove_dir(_sub_dir);

    //	Drop the subformat, if any.

    _subformat = NULL;

    //  Send an event to everybody who cares.

    CallBack::activate(Event(Event::Dismount, NULL, this));
}

void
Volume::share()
{
    assert(_is_shared);

    //  First, try exporting using options in /etc/exports.

    const char *exportfs_argv[8] = {
	"/usr/etc/exportfs", dir(), NULL
    };
    if (execute(exportfs_argv) == 0)
	return;

    //  Second, try exporting read-only.

    const char *exportfs2_argv[8] = {
	"/usr/etc/exportfs", "-i", "-o", "ro", dir(), NULL
    };
    (void) execute(exportfs2_argv);
}

void
Volume::unshare()
{
    const char *exportfs_argv[8] = {
	"/usr/etc/exportfs", "-u", dir(), NULL
    };
    (void) execute(exportfs_argv);
}

int
Volume::execute(const char *argv[8])
{
    int status = 0;
#define STR(p) ((p) ? (p) : "")

    Log::debug("executing command %s %s %s %s %s %s %s %s",
	       STR(argv[0]), STR(argv[1]), STR(argv[2]), STR(argv[3]),
	       STR(argv[4]), STR(argv[5]), STR(argv[6]), STR(argv[7]));
    assert(argv[0][0] == '/');

    int pid = fork();
    if (pid < 0)
    {	Log::perror("fork failed");
	return -1;
    }
    if (pid == 0)
    {
	// Child

	execv(argv[0], (char *const *) argv);
	Log::perror("%s exec failed", argv[0]);
	_exit(1);
    }
    else
    {
	// Parent

	do
	{
	    if (waitpid(pid, &status, NULL) != pid)
	    {   Log::perror("%s wait failed", argv[0]);
		return -1;
	    }
	} while (!WIFEXITED(status));
	if (status != 0)
	    Log::debug("command failed with status %d", status);
    }
    return status;
}

//  find_photoCD_subformat() looks for a PhotoCD.  If it finds one,
//  it sets the _subformat field.  This method must be called when
//  the volume is mounted. We look for the presence of a file "info.pcd"
//  inside the directory "photo_cd", to ensure that this is a photo CD.

void
Volume::find_photoCD_subformat()
{
    assert(_state == VS_MOUNTED);

    //  Only ISO 9660 CDs can be PhotoCDs.

    if (strcmp(type(), "iso9660") != 0)
	return;

    char path[PATH_MAX];
    (void) sprintf(path, "%s/photo_cd/info.pcd", dir());

    struct stat statbuf;
 
    if (stat(path, &statbuf) == 0){
        Log::debug("This is a photo CD volume.");
	_subformat = MC_SBFMT_PHOTOCD;
    }
    return;
}

//  find_inst_subformat looks for an installation volume.
//  This is kind of a kludge, but I don't know of a better place
//  to put this.

void
Volume::find_inst_subformat()
{
    assert(_state == VS_MOUNTED);

    if (has_inst_files())
    {
	Log::debug("This is an inst volume.");
	_subformat = MC_SBFMT_INST;
    }
}


bool
Volume::has_inst_files() const
{
    assert(_state == VS_MOUNTED);

    char dist[PATH_MAX];
    (void) sprintf(dist, "%s/.distribution", dir());
    struct stat statbuf_unused;
    if (stat(dist, &statbuf_unused) == 0)
	return true;

    (void) sprintf(dist, "%s/dist", dir());
    DIR *dir = opendir(dist);
    if (!dir)
	return false;
    struct direct *entry;
    bool found = false;
    while (entry = readdir(dir))
    {   int l = entry->d_namlen;
	if (l > 4 && !strcmp(entry->d_name + l - 4, ".idb"))
	{   found = true;
	    break;
	}
    }
    closedir(dir);
    return found;
}

// devices_are_shared() returns true if all partitions are on shared devices.

bool
Volume::devices_are_shared() const
{
    for (unsigned int i = 0; i < npartitions(); i++)
	if (!partition(i)->device()->is_shared())
	    return false;
    return true;
}

//  A slight embellishment to mkdir(2).  Recursively creates
//  parent directories and logs errors to syslog.

int
Volume::make_dir(const char *path)
{
    assert(path != NULL && path[0] == '/'); // Absolute paths only

    //  Does the directory exist?

    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
    {
	if (errno != ENOENT)
	    Log::perror("can't access mount directory \"%s\"", path);
	char parent[PATH_MAX];
	strcpy(parent, path);
	char *p = strrchr(parent, '/');
	assert(p != NULL);
	if (p == parent)
	    p++;			// Root is "/", not "".
	*p = '\0';			// Trim last component off path
	int err = make_dir(parent);
	if (err != 0)
	    return err;
	if (mkdir(path, 0777) != 0)
	{   Log::perror("can't create mount directory \"%s\"", path);
	    return errno;
	}
    }
    return 0;
}

//  A slight embellishment to rmdir(2).  Logs errors to syslog.

int
Volume::remove_dir(const char *path)
{
    if (rmdir(path) != 0)
    {	Log::perror("can't remove subdirectory \"%s\"", path);
	return errno;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//  Mount Task

//  This is the mount task.  All Volumes are mounted from this task.
//  When we get here, we know that all Volumes on a newly inserted
//  Device have been found, so it's safe to calculate subdirectories
//  here.
//
//  We don't mount all Volumes.  If two Volumes "overlap", we only
//  mount the "first" one.  Let me explain what I mean by "overlap"
//  and "first".
//
//  "Overlap" means one of two things: either that some partition of
//  Volume A has the same sectors as some partition of Volume B or
//  that some partition of Volume A is on the same CD-ROM device as
//  some partition of Volume B.  Because CD-ROMs have to use different
//  modes to access different kinds of data, and our mount_* programs
//  don't coordinate which mode is selected, we never mount two
//  filesystems from the same CD-ROM.
//
//  "First" means the Volume that precedes overlapping volumes
//  based on these sort rules:
//
//   1. Primary sort key: a Volume mentioned in a "mount" command in
//	the config file precedes an unmentioned volume.
//
//   2.	Secondary sort key: volume type (ASCII order, modulo the
//	Adobe Type On Call CD hack).
//
//   3. Tertiary sort key: volume partition number.  WholeDisk
//	is -1, so WholeDisk precedes other partitions.

void
Volume::mount_proc(Task& /* task */, void * /* closure */ )
{
    Volume *vol;

    //  Calculate all volumes' sharing heuristic.

    for (vol = first(); vol; vol = vol->next())
	vol->set_shared(vol->is_shared_heuristic());

    //  Find volumes that can't be mounted; don't bother mounting them.

    for (vol = first(); vol; vol = vol->next())
	if (vol->needs_mount() && vol->cant_mount())
	    vol->_needs_mount = false;
        else if (vol->is_mounted() && vol->cant_mount())
	    vol->dismount(false);
	    

    //  Calculate the subdirectories and mount all remaining volumes.

    for (vol = first(); vol; vol = vol->next())
	if (vol->_needs_mount)
	{   vol->calc_sub_dir();
	    vol->mount();
	    vol->_needs_mount = false;
	}
}

//  Test whether a volume can't be mounted because it's preceded
//  by an overlapping volume.

bool
Volume::cant_mount() const
{
    for (Volume *that = first(); that; that = that->next())
	if (that != this && overlaps(*that))
	{
	    bool this_is_mentioned = _config->volume_is_mentioned(address());
	    bool that_is_mentioned = _config->volume_is_mentioned(that->address());
	    if (that_is_mentioned && !this_is_mentioned)
		return true;
	    if (this_is_mentioned && !that_is_mentioned)
		continue;
	    int delta = typecmp(that->type(), type());
	    adobe_type_on_call_hack(delta, that);
	    if (delta < 0)
		return true;
	    if (delta > 0)
		continue;
	    int my_partno = partition(0)->index();
	    int his_partno = that->partition(0)->index();
	    if (his_partno < my_partno)
		return true;
	}
    return false;
}

int
Volume::typecmp(const char *name0, const char *name1) const
{
    //	Stupid special case -- iso9660 precedes hfs.  Otherwise
    //	it's alphabetical.

    if (!strcmp(name0, "iso9660") && !strcmp(name1, "hfs"))
	return -1;
    if (!strcmp(name0, "hfs") && !strcmp(name1, "iso9660"))
	return 1;
    return strcmp(name0, name1);
}

//  Oh, my God!  This is so embarassing.
//
//  Sometime in 1994/1995 or so, Adobe figured out that the old mediad
//  defaults mounting a hybrid HFS/ISO CD-ROM as HFS (unlike all the
//  other Unix vendors that prefer ISO).  As far as I can tell, that
//  was not planned, it's just a side effect of the way cdromd was
//  written.  Anyway, because of that, Adobe put their SGI
//  distribution on the HFS part of their Type On Call CD-ROM, which
//  is the same CD for Mac, Windoze, Sun and SGI.  Then I went and
//  changed the default to ISO and broke it.
//
//  So the hack is, if we have a CD with both an ISO partition and an
//  HFS, and the volume label of each half matches "TOC_*",
//  then we prefer to mount it as HFS, not ISO.  I sure hope Adobe
//  doesn't change its volume label so it stops matching that regexp.

void
Volume::adobe_type_on_call_hack(int& delta, const Volume *that) const
{
    assert(overlaps(*that));

    if (is_toc() && that->is_toc())
    {
	// This is a Type On Call CD.

	if (!strcmp(type(), "iso9660") && !strcmp(that->type(), "hfs"))
	    delta = -1;			// prefer this
	else if (!strcmp(type(), "hfs") && !strcmp(that->type(), "iso9660"))
	    delta = +1;			// prefer that
    }
}

// Check whether the volume label starts with "TOC_".

bool
Volume::is_toc() const
{
    const char *lab = label();
    return lab != NULL && strncmp(lab, "TOC_", 4) == 0;
}

//  Test whether two volumes overlap.  Any two volumes on the same
//  CD-ROM always overlap.

bool
Volume::overlaps(const Volume& that) const
{
    //  Iterate over all partitions in both volumes.

    const Partition *my_part, *his_part;
    for (unsigned int i = 0; my_part = partition(i); i++)
    {
	__uint64_t my_start = my_part->start_sector();
	__uint64_t my_end = my_start + my_part->n_sectors();
	for (unsigned int j = 0; his_part = that.partition(j); j++)
	{
	    Device *my_device = my_part->device();
	    Device *his_device = his_part->device();
	    if (my_device != his_device)
		continue;

	    //	Problems with CD-ROM mode select mean we never mount
	    //	more than one partition of a CD-ROM.  So we declare
	    //	that any two partitions on the same CD-ROM overlap.

	    //	I have a dream of a day when this restriction can
	    //	be removed.

	    if (my_device->filesys_cdda())
		return true;


	    // Test whether this pair of partitions overlaps.

	    __uint64_t his_start = his_part->start_sector();
	    __uint64_t his_end = his_start + his_part->n_sectors();
	    if (my_start < his_end && my_end > his_start)
		return true;
	}
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
//  Config Task

//  This is the config task.  Whenever the config file changes, this
//  task sets the appropriate fields in the Volume.  The Volume's
//  set_* methods cleverly dismount or remount the Volume if
//  it's needed.

void
Volume::config_proc(Config& config, void *closure)
{
    Volume *vol = (Volume *) closure;
    const VolumeAddress& va = vol->address();

    //	Set the mount directory/options.

    vol->set_config_dir(config.mount_dir(va));
    vol->set_config_options(config.mount_options(va));

    //	Set whether the volume is ignored.  If it is not ignored, and
    //	if one of the preceding set_ methods dismounted the volume,
    //	set_ignored() will schedule it for remounting.

    vol->set_ignored(config.volume_is_ignored(va));

    //	Schedule the mount proc.  The mount proc may figure out that
    //  there's more to be done.

    mount_task.schedule(Task::ASAP);
}
