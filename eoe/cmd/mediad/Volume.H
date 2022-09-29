#ifndef Volume_included
#define Volume_included

#include "Enumerable.H"
#include "VolumeAddress.H"
#include "bool.H"

class Config;
class Partition;
class Task;
struct mntent;

//  A Volume is an abstract base class representing a filesystem.
//  Currently, the only concrete derived class is SimpleVolume,
//  but perhaps someday there will be derived classes for logical
//  volumes or xlvs.  A volume mounts and dismounts itself.
//
//  A Volume has a lot of state.  It has a VolumeAddress, a label, a
//  filesystem name, a type, several directories, a couple of mount
//  options strings and several flags that control whether it should
//  be mounted.  The directories are described below.
//
//  The label is a format-dependent character string.  DOS and HFS
//  volumes have labels.  EFS volumes do not.
//
//  See the comments in/near Volume::options() for an explanation
//  of mount options.
//
//  Each Volume monitors the config file.  When the config file
//  changes, the volume may mount itself, dismount itself or dismount
//  and remount itself.
//
//  DIRECTORIES
//
//  A Volume has several directory names associated with it.  They are
//  the default directory, the base directory, the subdirectory, the
//  config directory, and "The Directory".
//
//  The default directory is where the volume should be mounted
//  in the absence of config files or conflicts.  For a SimpleVolume,
//  the default directory is constructed by prepending "/" to the
//  device name, e.g., "/jaz3" for the third Jaz drive.  The
//  default directory is always defined.
//
//  The base directory is the directory from which the subdirectory
//  may be constructed.  Currently, it's the same as the default
//  directory.
//
//  If more than one volume has the same base directory,
//  the volumes all have a subdirectory.  For a SimpleVolume,
//  the subdirectory is constructed by appending "/<type>.partition<N>"
//  to the default directory.  E.g., "/jaz3/dos.partition6" for
//  the sixth DOS partition on the third Jaz drive.  If there
//  are no other volumes with the same base directory, the
//  subdirectory is NULL.
//
//  The config directory is where the configuration file says
//  the volume should be mounted.  If specified,
//  the config directory overrides the subdirectory and default
//  directory.  If it is not specified, it will be NULL.
//
//  "The Directory" is where the volume will actually be mounted.
//  It is the config directory, if defined, otherwise it's the
//  subdirectory if that's defined, otherwise it's the default directory.

class Volume : private Enumerable {

public:

    // XXX Security fix late in IRIX 6.5 release.
    static void setGlobalOptions(const char* globalOptions)
    {
	_globalOptions = globalOptions;
    }

    Volume(const VolumeAddress&, const mntent&, const char *label);
    virtual ~Volume();

    virtual unsigned int npartitions() const = 0;
    virtual const Partition *partition(unsigned int) const = 0;

    //  Data Access Methods

    const VolumeAddress& address() const { return _address; }
    const char *label() const		{ return _label; }
    const char *fsname() const		{ return _fsname; }
    const char *type() const		{ return _type; }

    bool is_mounted() const		{ return _state == VS_MOUNTED; }
    bool is_ignored() const		{ return _is_ignored; }
    bool is_shared() const		{ return _is_shared; }
    bool needs_mount() const		{ return _needs_mount; }
    bool forced_unmount() const		{ return _forced_unmount; }

    const char *default_dir() const	{ return _default_dir; }
    const char *config_dir() const	{ return _config_dir; }
    const char *sub_dir() const		{ return _sub_dir; }
    const char *base_dir() const	{ return _default_dir; }
    const char *dir() const;
    const char *options();		// not const
    const char *subformat() const	{ return _subformat; }

    // Data Modification Methods

    void set_ignored(bool);
    void set_shared(bool);
    void set_config_dir(const char *);
    void set_sub_dir(const char *);
    void set_config_options(const char *);

    //  Virtual Methods

    virtual void mount();
    virtual void dismount(bool force);
    virtual void share();
    virtual void unshare();
    virtual int  execute(const char *argv[8]);
    virtual void find_photoCD_subformat();
    virtual void find_inst_subformat();
    virtual void calc_sub_dir();
    virtual void use_subdir() = 0;
    virtual bool devices_are_shared() const;

    //  Volume enumeration

    ENUMERATION_METHODS(Volume, volumes);

private:

    enum State { VS_NOT_MOUNTED, VS_MOUNTED, VS_MOUNT_FAILED };

    //  Instance Variables

    VolumeAddress _address;
    char	 *_label;
    char	 *_fsname;
    char	 *_type;
    State	  _state;
    bool	  _is_ignored;
    bool	  _is_shared;
    bool	  _needs_mount;
    bool	  _forced_unmount;	// a forced umount event is in progress

    char	 *_default_dir;
    char	 *_config_dir;
    char	 *_sub_dir;

    char	 *_default_opts;
    char	 *_config_opts;
    char	 *_all_opts;

    const char	 * _subformat;

    Config	 *_config;		// pointer so we don't include Config.H

    // XXX Security fix late in IRIX 6.5 release.
    static const char	 *_globalOptions;

    //  Class Variables

    static Enumerable::Set volumes;
    static Task mount_task;

    //  Private Class Methods

    static char *mntopt(char **);
    static void mount_proc(Task&, void *);
    static void config_proc(Config&, void *);

    //  Private Instance Methods

    int make_dir(const char *path);
    int remove_dir(const char *path);
    bool has_inst_files() const;
    virtual bool is_shared_heuristic() const;

    Volume(const Volume&);		// Do not copy
    void operator = (const Volume&);	//  or assign.

    bool cant_mount() const;
    int typecmp(const char *, const char *) const;
    bool overlaps(const Volume& that) const;
    bool dir_collides();
    bool is_toc() const;
    void adobe_type_on_call_hack(int&, const Volume *) const;

};

#endif /* !Volume_included */
