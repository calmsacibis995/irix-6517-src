#ifndef FileSystemTable_included
#define FileSystemTable_included

#include <sys/types.h>			// define ulong_t

#include "SmallTable.h"
#include "StringTable.h"

class Cred;
class Event;
class FileSystem;
class InternalClient;

//  FileSystemTable provides a static function, find(), which looks up
//  a path and returns a pointer to the FileSystem where that path
//  resides.

class FileSystemTable {

public:

#ifdef NO_LEAKS
    FileSystemTable();
    ~FileSystemTable();
#endif

    static FileSystem *find(const char *path, const Cred& cr); 

private:

    typedef SmallTable<ulong_t, FileSystem *> IDTable;
    typedef StringTable<FileSystem *> NameTable;

    //  Class Variables

    static const char mtab_name[];
    static unsigned int count;
    static IDTable    fs_by_id;
    static NameTable *fs_by_name;
    static InternalClient *mtab_watcher;
    static FileSystem *root;

    //  Class Methods

    static void create_fs_by_name();
    static void destroy_fses(NameTable *);
    static FileSystem *longest_prefix(const char *path);
    static void mtab_event_handler(const Event&, void *);

};

#ifdef NO_LEAKS
static FileSystemTable FileSystemTable_instance;
#endif

#endif /* !FileSystemTable_included */
