#ifndef Fsd_defined
#define Fsd_defined

#include "bool.H"
#include "FAMonitor.H"

struct mntent;

//  I have no idea what FSD stands for, but it's used in /etc/fsd.auto
//  and /etc/fsd.tab.

class Fsd {
    
public:

    Fsd(const char *name);
    ~Fsd();

    const mntent *operator [] (unsigned int) const;
          mntent *operator [] (unsigned int);
    const mntent *at_fsname_type(const char *, const char *) const;
          mntent *at_fsname_type(const char *, const char *);
    const mntent *at_dir(const char *) const;
          mntent *at_dir(const char *);

    void replace(unsigned int, const mntent *);
//    void remove(unsigned int);
    void append(const mntent *);

    static Fsd& fsd_auto();
//    static Fsd& fsd_tab();

private:

    char *_path;
    unsigned int _nentries;
    mntent *_entries;
    FileMonitor _mon;
    bool _end_exist_seen;

    static Fsd *_auto;
//    static Fsd *_tab;

    void read();
    void destroy();

    static void change_proc(FAMonitor&, const FAMEvent&, void *);

    Fsd(const Fsd&);			// Do not copy
    void operator = (const Fsd&);	//  or assign.

};

#endif /* !Fsd_defined */
