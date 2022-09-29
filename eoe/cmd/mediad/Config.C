#include "Config.H"

#include <assert.h>
#include <ctype.h>
#include <fam.h>
#include <mntent.h>
#include <mediad.h>			// for DEFAULT_*CHK_INTERVAL
#include <stdio.h>
#include <string.h>

#include "DeviceAddress.H"
#include "FAMonitor.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "VolumeAddress.H"
#include "operators.H"			// for config::operator !=
#include "strplus.H"

//  the config class and its fu are defined local to this file instead
//  of in the Config class because I don't want Config.H to include
//  VolumeAddress.H or DeviceAddress.H.

class config {

public:

    struct monitor_device {
	DeviceAddress da;
	int inschk;
	int rmvchk;
    };

    struct mount_volume {
	VolumeAddress va;
	char *dir;
	char *opts;
	bool is_shared;
	bool is_unshared;
    };

    struct share_device {
	DeviceAddress da;
    };

    config();
    ~config();

    // Accessors

    bool device_is_ignored(const DeviceAddress&) const;
    bool volume_is_ignored(const VolumeAddress&) const;
    bool device_is_shared(const DeviceAddress&) const;
    const monitor_device *best_match(const DeviceAddress&) const;
    const mount_volume *best_match(const VolumeAddress&) const;

    bool operator == (const config&) const;

    // Modifiers

    void add_ignore_device(const DeviceAddress&);
    void add_ignore_volume(const VolumeAddress&);
    void add_monitor_device(const DeviceAddress&, int inschk, int rmvchk);
    void add_mount_volume(const VolumeAddress&,
			  const char *dir, const char *opts,
			  bool is_shared, bool is_unshared);
    void add_share_device(const DeviceAddress&);

private:

    DeviceAddress *ignore_devices;
    VolumeAddress *ignore_volumes;
    monitor_device *monitor_devices;
    mount_volume *mount_volumes;
    share_device *share_devices;

    unsigned int n_ignore_devices;
    unsigned int n_ignore_volumes;
    unsigned int n_monitor_devices;
    unsigned int n_mount_volumes;
    unsigned int n_share_devices;

    unsigned int n_ignore_devices_alloc;
    unsigned int n_ignore_volumes_alloc;
    unsigned int n_monitor_devices_alloc;
    unsigned int n_mount_volumes_alloc;
    unsigned int n_share_devices_alloc;

    config(const config&);		// Do not copy
    void operator = (const config&);   	//  or assign.

};

static config *current_config;
static FILE *cf;

Enumerable::Set Config::all_configs;
bool            Config::initialized = false;
FileMonitor    *Config::config_monitor;
const char     *Config::config_path = "/etc/config/mediad.config";

Config::Config(ConfigProc proc, void *closure)
: Enumerable(all_configs),
  _proc(proc),
  _closure(closure)
{
    if (!initialized)
	init();
}

Config::~Config()
{ }

bool
Config::device_is_ignored(const DeviceAddress& da)
{
    return current_config ? current_config->device_is_ignored(da) : false;
}

bool
Config::volume_is_ignored(const VolumeAddress& va)
{
    return current_config ? current_config->volume_is_ignored(va) : false;
}

bool
Config::device_is_mentioned(const DeviceAddress& da)
{
    if (current_config && current_config->best_match(da))
	return true;
    else
	return false;
}

// N.B. This returns true iff the volume is mentioned by name in the
// config file.

bool
Config::volume_is_mentioned(const VolumeAddress& va)
{
    if (current_config && current_config->best_match(va))
	return true;
    else
	return false;
}

bool
Config::device_is_shared(const DeviceAddress& da)
{
    return current_config ? current_config->device_is_shared(da) : false;
}

unsigned int
Config::device_inschk(const DeviceAddress& da)
{
    const config::monitor_device *dev = NULL;
    if (current_config)
	dev = current_config->best_match(da);
    if (dev)
	return dev->inschk;
    else
	return DEFAULT_INSCHK_INTERVAL;
}

unsigned int
Config::device_rmvchk(const DeviceAddress& da)
{
    const config::monitor_device *dev = NULL;
    if (current_config)
	dev = current_config->best_match(da);
    if (dev)
	return dev->rmvchk;
    else
	return DEFAULT_RMVCHK_INTERVAL;
}

const char *
Config::mount_dir(const VolumeAddress& va)
{
    const config::mount_volume *vol = NULL;
    if (current_config)
	vol = current_config->best_match(va);
    if (vol)
	return vol->dir;
    else
	return NULL;
}

const char *
Config::mount_options(const VolumeAddress& va)
{
    const config::mount_volume *vol = NULL;
    if (current_config)
	vol = current_config->best_match(va);
    if (vol)
	return vol->opts;
    else
	return NULL;
}

bool
Config::mount_is_shared(const VolumeAddress& va)
{
    const config::mount_volume *vol = NULL;
    if (current_config)
	vol = current_config->best_match(va);
    if (vol)
	return vol->is_shared;
    else
	return false;
}

bool
Config::mount_is_unshared(const VolumeAddress& va)
{
    const config::mount_volume *vol = NULL;
    if (current_config)
	vol = current_config->best_match(va);
    if (vol)
	return vol->is_unshared;
    else
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// config methods

config::config()
{
    ignore_devices = NULL;
    ignore_volumes = NULL;
    monitor_devices = NULL;
    mount_volumes = NULL;
    share_devices = NULL;

    n_ignore_devices = 0;
    n_ignore_volumes = 0;
    n_monitor_devices = 0;
    n_mount_volumes = 0;
    n_share_devices = 0;

    n_ignore_devices_alloc = 0;
    n_ignore_volumes_alloc = 0;
    n_monitor_devices_alloc = 0;
    n_mount_volumes_alloc = 0;
    n_share_devices_alloc = 0;
}

config::~config()
{
    for (unsigned int i = 0; i < n_mount_volumes; i++)
    {	delete [] mount_volumes[i].dir;
	delete [] mount_volumes[i].opts;
    }
    delete [] ignore_devices;
    delete [] ignore_volumes;
    delete [] monitor_devices;
    delete [] mount_volumes;
    delete [] share_devices;
}

bool
config::device_is_ignored(const DeviceAddress& da) const
{
    for (unsigned int i = 0; i < n_ignore_devices; i++)
	if (ignore_devices[i] == da)
	    return true;
    return false;
}

bool
config::volume_is_ignored(const VolumeAddress& va) const
{
    for (unsigned int i = 0; i < n_ignore_volumes; i++)
	if (ignore_volumes[i] == va)
	    return true;
    return false;
}

bool
config::device_is_shared(const DeviceAddress& da) const
{
    for (unsigned int i = 0; i < n_share_devices; i++)
	if (share_devices[i].da == da)
	    return true;
    return false;
}

const config::monitor_device *
config::best_match(const DeviceAddress& da) const
{
    for (unsigned i = 0; i < n_monitor_devices; i++)
	if (monitor_devices[i].da == da)
	    return &monitor_devices[i];
    return NULL;
}

const config::mount_volume *
config::best_match(const VolumeAddress& va) const
{
    for (unsigned i = 0; i < n_mount_volumes; i++)
	if (mount_volumes[i].va == va)
	    return &mount_volumes[i];
    return NULL;
}

bool
config::operator == (const config& that) const
{
    unsigned int i;

    // Quick checks

    if (this == &that)
	return true;
    if (n_ignore_devices != that.n_ignore_devices)
	return false;
    if (n_ignore_volumes != that.n_ignore_volumes)
	return false;
    if (n_monitor_devices != that.n_monitor_devices)
	return false;
    if (n_mount_volumes != that.n_mount_volumes)
	return false;
    if (n_share_devices != that.n_share_devices)
	return false;
    
    // Compare ignore_devices.

    for (i = 0; i < n_ignore_devices; i++)
	if (!that.device_is_ignored(ignore_devices[i]))
	    return false;

    // Compare ignore_volumes.

    for (i = 0; i < n_ignore_volumes; i++)
	if (!that.volume_is_ignored(ignore_volumes[i]))
	    return false;

    // Compare monitor_devices.

    for (i = 0; i < n_monitor_devices; i++)
    {   const monitor_device *here = &monitor_devices[i];
	const monitor_device *there = that.best_match(here->da);
	if (!there ||
	    here->da != there->da ||
	    here->inschk != there->inschk ||
	    here->rmvchk != there->rmvchk)
	    return false;
    }

    // Compare mount_volumes.

    for (i = 0; i < n_mount_volumes; i++)
    {   const mount_volume *here = &mount_volumes[i];
	const mount_volume *there = that.best_match(here->va);
	if (!there)
	    return false;
	if (here->va != there->va)
	    return false;
	if (here->dir && !there->dir || there->dir && !here->dir)
	    return false;
	if (here->dir && there->dir && strcmp(here->dir, there->dir))
	    return false;
	if (here->opts && !there->opts || there->opts && !here->opts)
	    return false;
	if (here->opts && there->opts && strcmp(here->opts, there->opts))
	    return false;
	if (here->is_shared != there->is_shared)
	    return false;
	if (here->is_unshared != there->is_unshared)
	    return false;
    }

    // Compare share_devices

    for (i = 0; i < n_share_devices; i++)
	if (!that.device_is_shared(share_devices[i].da))
	    return false;

    return true;
}

void
config::add_ignore_device(const DeviceAddress& da)
{
    for (unsigned int i = 0; i < n_ignore_devices; i++)
	if (ignore_devices[i] == da)
	{   Log::error("same device ignored twice in /etc/mediad/mediad.config");
	    return;
	}

    if (n_ignore_devices >= n_ignore_devices_alloc)
    {   n_ignore_devices_alloc = 2 * n_ignore_devices_alloc + 3;
	DeviceAddress *p = new DeviceAddress[n_ignore_devices_alloc];
	for (unsigned int i = 0; i < n_ignore_devices; i++)
	    p[i] = ignore_devices[i];
	delete [] ignore_devices;
	ignore_devices = p;
    }
    ignore_devices[n_ignore_devices++] = da;
}

void
config::add_ignore_volume(const VolumeAddress& va)
{
    for (unsigned int i = 0; i < n_ignore_volumes; i++)
	if (ignore_volumes[i] == va)
	{   Log::error("same filesystem ignored twice in /etc/mediad/mediad.config");
	    return;
	}

    if (n_ignore_volumes >= n_ignore_volumes_alloc)
    {   n_ignore_volumes_alloc = 2 * n_ignore_volumes_alloc + 3;
	VolumeAddress *p = new VolumeAddress[n_ignore_volumes_alloc];
	for (unsigned int i = 0; i < n_ignore_volumes; i++)
	    p[i] = ignore_volumes[i];
	delete [] ignore_volumes;
	ignore_volumes = p;
    }
    ignore_volumes[n_ignore_volumes++] = va;
}

void
config::add_monitor_device(const DeviceAddress& da, int inschk, int rmvchk)
{
    for (unsigned int i = 0; i < n_monitor_devices; i++)
	if (monitor_devices[i].da == da)
	{   Log::error("same device monitored twice in /etc/mediad/mediad.config");
	    return;
	}

    if (n_monitor_devices >= n_monitor_devices_alloc)
    {   n_monitor_devices_alloc = 2 * n_monitor_devices_alloc + 3;
	monitor_device *p = new monitor_device[n_monitor_devices_alloc];
	for (unsigned int i = 0; i < n_monitor_devices; i++)
	    p[i] = monitor_devices[i];
	delete [] monitor_devices;
	monitor_devices = p;
    }
    monitor_devices[n_monitor_devices].da = da;
    monitor_devices[n_monitor_devices].inschk = inschk;
    monitor_devices[n_monitor_devices].rmvchk = rmvchk;
    n_monitor_devices++;
}


void
config::add_mount_volume(const VolumeAddress& va,
			 const char *dir, const char *opts,
			 bool is_shared, bool is_unshared)
{
    for (unsigned int i = 0; i < n_mount_volumes; i++)
	if (mount_volumes[i].va == va)
	{   Log::error("same filesystem mounted twice in /etc/mediad/mediad.config");
	    return;
	}

    if (n_mount_volumes >= n_mount_volumes_alloc)
    {   n_mount_volumes_alloc = 2 * n_mount_volumes_alloc + 3;
	mount_volume *p = new mount_volume[n_mount_volumes_alloc];
	for (unsigned int i = 0; i < n_mount_volumes; i++)
	    p[i] = mount_volumes[i];
	delete [] mount_volumes;
	mount_volumes = p;
    }
    mount_volumes[n_mount_volumes].va = va;
    mount_volumes[n_mount_volumes].dir =
				     (dir && *dir) ? strdupplus(dir) : NULL;
    mount_volumes[n_mount_volumes].opts =
				     (opts && *opts) ? strdupplus(opts) : NULL;
    mount_volumes[n_mount_volumes].is_shared = is_shared;
    mount_volumes[n_mount_volumes].is_unshared = is_unshared;
    n_mount_volumes++;
}

void
config::add_share_device(const DeviceAddress& da)
{
    for (unsigned int i = 0; i < n_share_devices; i++)
	if (share_devices[i].da == da)
	{   Log::error("same device shared twice in /etc/mediad/mediad.config");
	    return;
	}

    if (n_share_devices >= n_share_devices_alloc)
    {   n_share_devices_alloc = 2 * n_share_devices_alloc + 3;
	share_device *p = new share_device[n_share_devices_alloc];
	for (unsigned int i = 0; i < n_share_devices; i++)
	    p[i] = share_devices[i];
	delete [] share_devices;
	share_devices = p;
    }
    share_devices[n_share_devices++].da = da;
}

///////////////////////////////////////////////////////////////////////////////
// initialization and config file parsing.

enum Token {
    DEVICE,
    DIRECTORY,
    FILESYSTEM,
    IGNORE,
    INSCHK,
    MONITOR,
    MOUNT,
    OPTIONS,
    PARTITION,
    RMVCHK,
    SHARE,
    SHARED,
    UNSHARED,

    PATH,
    NUMBER,
    STRING,
    ENDLINE,
    ENDFILE
};

union YYSTYPE {
    int number;
    char string[MNTMAXSTR];
} yylval;

static bool token_is_saved = false;
static Token last_token;
static unsigned int line_no;
static char linebuffer[1024];
static int ungot_char = EOF;
static char *charp;

int
Config::yygetchar()
{
    assert(charp == NULL || charp >= linebuffer && charp < linebuffer + sizeof linebuffer);
    if (ungot_char != EOF)
    {   int c = ungot_char;
	ungot_char = EOF;
	return c;
    }
    if (!charp || !*charp)
    {
	assert(cf != NULL);
	charp = fgets(linebuffer, sizeof linebuffer, cf);
	line_no++;
    }
    if (!charp)
	return EOF;
    return *charp++;
}

void
Config::yyungetchar(int c)
{
    ungot_char = c;
}

void
Config::error(const char *msg)
{
    Log::error("error parsing %s: %s", config_path, msg);
    if (charp)
	Log::error("context: line %d after \"%.*s\"",
		   line_no, charp - linebuffer, linebuffer);
    else
	Log::error("context: end of file %s", config_path);
}

void
Config::yyunlex()
{
    assert(!token_is_saved);
    token_is_saved = true;
}

Token
Config::yylex()
{
    if (token_is_saved)
    {   token_is_saved = false;
	return last_token;
    }

    struct kw {
	char *name;
	Token value;
    };

    int i, c;
    struct kw *p;
    static struct kw keywords[] = {
	{ "device",     DEVICE },
	{ "directory",  DIRECTORY },
	{ "filesystem", FILESYSTEM },
	{ "ignore",     IGNORE },
	{ "inschk",     INSCHK },
	{ "monitor",    MONITOR },
	{ "mount",      MOUNT },
	{ "options",    OPTIONS },
	{ "partition",  PARTITION },
	{ "rmvchk",     RMVCHK },
	{ "share",      SHARE },
	{ "shared",     SHARED },
	{ "unshared",   UNSHARED },
	{ NULL }
    };

    do
	c = yygetchar();
    while (c == ' ' || c == '\t');	// Skip whitespace.  (not newlines)
	  

    if (c == '#')			// Skip comments.
	while ((c = yygetchar()) != EOF && c != '\n')
	    continue;
    // Fall through after skipping comment...

    if (c == EOF)
	return last_token = ENDFILE;
    else if (c == '\n')
	return last_token = ENDLINE;

    if (isascii(c) && isdigit(c))	// Scan a number.
    {   int n = 0;
	do
	{   n = 10 * n + c - '0';
	    c = yygetchar();
	} while (isascii(c) && isdigit(c));
	yyungetchar(c);
	yylval.number = n;
	return last_token = NUMBER;
    }

    // Whatever is left must be a path, a string or a keyword.

    i = 0;
    do
    {   if (i < sizeof yylval.string - 1)
	    yylval.string[i++] = c;
	c = yygetchar();
    } while (isascii(c) && !isspace(c));
    yyungetchar(c);
    yylval.string[i] = '\0';

    // Is it a keyword?

    for (p = keywords; p->name != NULL; p++)
	if (!strcmp(yylval.string, p->name))
	    return last_token = p->value;

    // Not a keyword.  Either a path or a string.

    return last_token = (yylval.string[0] == '/' ? PATH : STRING);
}

bool
Config::parse_device(DeviceAddress *da)
{
    Token token = yylex();
    if (token == PATH)
    {
	// Convert path to a VolumeAddress.  Verify that the VolumeAddress
	// refers to a single partition, then extract the device part
	// of that partition's address.

	VolumeAddress va(yylval.string);
	if (va.valid() && !va.partition(1))
	    *da = va.partition(0)->device();
	return true;
    }
    else
    {	error("expected a path");
	return false;
    }
}

FormatIndex
Config::match_fstype(const char *type)
{
    struct {
	char *name;
	FormatIndex index;
    } table[] = {
	{ "audio", FMT_CDDA },
	{ "efs", FMT_EFS },
	{ "hfs", FMT_HFS },
	{ "mac", FMT_HFS },
	{ "dos", FMT_DOS },
	{ "fat", FMT_DOS },
	{ "xfs", FMT_XFS },
	{ "iso9660", FMT_ISO },
	{ "cdda", FMT_CDDA },
	{ "music", FMT_CDDA },
	{ NULL, FMT_UNKNOWN }
    }, *p;

    for (p = table; p->name && strcmp(p->name, type); p++)
	continue;
    return p->index;
}


//  Volume description has two forms.
//	path type
//	path type partition N

bool
Config::parse_volume(VolumeAddress *va)
{
    char path[MNTMAXSTR];
    Token token = yylex();
    if (token != PATH)
    {	error("expected a path");
	return false;
    }
    strcpy(path, yylval.string);
    token = yylex();
    if (token != STRING)
    {	error("expected a filesystem type");
	return false;
    }
    FormatIndex type = match_fstype(yylval.string);
    if (type == FMT_UNKNOWN)
    {   error("unrecognized filesystem type");
	return false;
    }
    int partno = PartitionAddress::UnknownPartition;
    token = yylex();
    if (token == PARTITION)
    {   token = yylex();
	if (token != NUMBER)
	{   error("expected a number");
	    return false;
	}
	partno = yylval.number;
    }
    else
	yyunlex();
    *va = VolumeAddress(path, type, partno);
    return true;
}

bool
Config::parse_line(config *config)
{
    DeviceAddress da;
    VolumeAddress va;

    Token token = yylex();
    if (token == ENDFILE)
	return false;

    if (token == IGNORE)
    {   token = yylex();
	if (token == DEVICE)
	{
	    if (parse_device(&da))
		config->add_ignore_device(da);
	}
	else if (token == FILESYSTEM)
	{
	    if (parse_volume(&va))
		config->add_ignore_volume(va);
	}
	else
	{   error("expected `device' or `filesystem'");
	    return false;
	}
	token = yylex();
    }
    else if (token == MONITOR)
    {   token = yylex();
	if (token != DEVICE)
	{   error("expected `device'");
	    return false;
	}
	if (!parse_device(&da))
	    return false;
	int inschk = DEFAULT_INSCHK_INTERVAL;
	int rmvchk = DEFAULT_RMVCHK_INTERVAL;
	while ((token = yylex()) != ENDLINE)
	    if (token == INSCHK)
	    {   token = yylex();
		if (token != NUMBER)
		{   error("expected a number");
		    return false;
		}
		inschk = yylval.number;
	    }
	    else if (token == RMVCHK)
	    {   token = yylex();
		if (token != NUMBER)
		{   error("expected a number");
		    return false;
		}
		rmvchk = yylval.number;
	    }
	    else
	    {	error("expected `inschk' or `rmvchk'");
		return false;
	    }
	config->add_monitor_device(da, inschk, rmvchk);
    }
    else if (token == MOUNT)
    {   token = yylex();
	if (token != FILESYSTEM)
	{   error("expected `filesytem'");
	    return false;
	}
	if (!parse_volume(&va))
	    return false;
	char dir[MNTMAXSTR] = "";
	char opts[MNTMAXSTR] = "";
	bool is_shared = false, is_unshared = false;
	while ((token = yylex()) != ENDLINE)
	    if (token == DIRECTORY)
	    {   token = yylex();
		if (token != PATH)
		{   error("expected a directory");
		    return false;
		}
		strcpy(dir, yylval.string);
	    }
	    else if (token == OPTIONS)
	    {   token = yylex();
		if (token != STRING)
		{   error("expected an options string");
		    return false;
		}
		strcpy(opts, yylval.string);
	    }
	    else if (token == SHARED)
		is_shared = true;
	    else if (token == UNSHARED)
		is_unshared = true;
	    else
	    {	error("expected `directory' or `options'");
		return false;
	    }
	if (is_shared && is_unshared)
	{   Log::error("filesystem can't be both shared and unshared");
	    return false;
	}
	config->add_mount_volume(va, dir, opts, is_shared, is_unshared);
    }
    else if (token == SHARE)
    {	token = yylex();
	if (token == DEVICE)
	{   if (!parse_device(&da))
		return false;
	    config->add_share_device(da);
	}
	else
	{   error("expected `device'");
	    return false;
	}
	token = yylex();		// Get ENDLINE.
    }
    else if (token != ENDLINE)
    {
	error("expected `ignore', `monitor', `mount' or `share'");
	return false;
    }
    return token == ENDLINE;
}

config *
Config::parse_config()
{
    cf = fopen(config_path, "r");
    if (!cf)
	return NULL;
    config *p = new config;
    line_no = 0;
    token_is_saved = false;
    charp = 0;
    ungot_char = EOF;
    while (parse_line(p))
	continue;
    fclose(cf);
    cf = NULL;
    if (last_token != ENDFILE)		// syntax error
    {	delete p;
	p = NULL;
    }
    return p;
}

void
Config::init()
{
    assert(!initialized);
    initialized = true;
    current_config = parse_config();
    config_monitor = new FileMonitor(config_path, change_proc, NULL);
}

void
Config::change_proc(FAMonitor&, const FAMEvent& event, void *)
{
    bool broadcast_needed = false;

    if (event.code != FAMExists && event.code != FAMEndExist &&
	event.code != FAMAcknowledge &&
	(event.code != FAMDeleted || current_config != NULL))
    {   
	Log::debug("%s changed. Rereading.", config_path);
	config *new_config = parse_config();
	if (new_config && current_config && *new_config != *current_config ||
	    new_config || current_config)
	{   delete current_config;
	    current_config = new_config;
	    broadcast_needed = true;
	}
    }

    // Tell everybody that something changed.

    if (broadcast_needed)
	for (Config *p = first(); p; p = p->next())
	    (*p->_proc)(*p, p->_closure);
}
