#ifndef Interest_included
#define Interest_included

#include <sys/stat.h>

#include "Boolean.h"

class ChangeFlags;
class Event;
class FileSystem;
class IMon;
struct stat;

//  Interest -- abstract base class for filesystem entities of interest.
//
//  An Interest is monitored by imon or, if imon fails, it is polled
//  by the Pollster.
//
//  All Interests are kept in a global table keyed by dev/ino.
//
//  The classes derived from Interest are...
//
//	ClientInterest		an Interest a Client has explicitly monitored
//	Directory		a directory
//	File			a file
//	DirEntry		an entry in a monitored directory

class Interest {

public:

    Interest(const char *name, FileSystem *);
    virtual ~Interest();

    const char *name() const		{ return myname; }
    Boolean exists() const		{ return old_stat.mode != 0; }
    Boolean isdir() const    { return (old_stat.mode & S_IFMT) == S_IFDIR; }
    virtual Boolean active() const = 0;
    Boolean needs_scan() const		{ return scan_state != OK; }
    void needs_scan(Boolean tf)		{ scan_state = tf ? NEEDS_SCAN : OK; }
    void mark_for_scan()		{ needs_scan(true); }
    virtual void do_scan();
    void report_exec_state();

    virtual void scan(Interest * = 0) = 0;
    virtual void unscan(Interest * = 0) = 0;
    void poll()				{ scan(); }

    //  Public Class Method

    static void imon_handler(dev_t, ino_t, int event);

protected:

    ChangeFlags do_stat();
    virtual void post_event(const Event&, const char * = NULL) = 0;
    char& ci_bits()			{ return ci_char; }
    char& dir_bits()			{ return dir_char; }
    const char& ci_bits() const		{ return ci_char; }
    const char& dir_bits() const	{ return dir_char; }

private:

    enum { HASHSIZE = 257 };
    enum ScanState	{ OK, NEEDS_SCAN };
    enum ExecState	{ EXECUTING, NOT_EXECUTING };
    struct compressed_stat {
	u_short mode;
	off_t size;			// first in case it's 64 bits
	uid_t uid;
	gid_t gid;
	timeval mtime;
	timeval ctime;
    };

    //  Instance Variables

    Interest *hashlink;
    dev_t dev;
    ino_t ino;
    char *const myname;
    ScanState     scan_state: 1;
    ExecState cur_exec_state: 1;
    ExecState old_exec_state: 1;
    char ci_char;
    char dir_char;
    struct compressed_stat old_stat;

    //  Private Instance Methods

    void dev_ino(dev_t, ino_t);
    void revoke();
//    ChangeFlags stat_diff(struct stat&);
    ChangeFlags update_stat(struct stat&);
    virtual void notify_created(Interest *) = 0;
    virtual void notify_deleted(Interest *) = 0;

    //  Class Variables

    static IMon imon;
    static Interest *hashtable[HASHSIZE];

    //  The Hashing Function

    static Interest **hashchain(dev_t d, ino_t i)
			  { return &hashtable[(unsigned) (d + i) % HASHSIZE]; }
    Interest **hashchain() const	{ return hashchain(dev, ino); }

    Interest(const Interest&);		// Do not copy
    operator = (const Interest&);	//  or assign.

};

#endif /* !Interest_included */
