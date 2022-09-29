#ifndef FAMonitor_included
#define FAMonitor_included

#include "bool.H"

struct FAMConnection;
struct FAMEvent;
class ReadHandler;

class FAMonitor {

public:

    typedef void (*ChangeProc)(FAMonitor&, const FAMEvent&, void *);

protected:

    enum Type { FILE, DIR };

    FAMonitor(const char *, Type, ChangeProc, void *);
    virtual ~FAMonitor();

private:

    enum { MAX_REQUESTS = 3 };

    struct Request {
	bool in_use;
	FAMonitor *mon;
    };

    //  Instance Variables

    int _reqnum;
    ChangeProc _proc;
    void *_closure;

    //  Class Variables

    static FAMConnection fam;		//  Undeclared - don't #include <fam.h>
    static Request requests[MAX_REQUESTS];
    static bool connected;
    static ReadHandler event_handler;

    //  Private Instance Methods

    void post(const FAMEvent& e)	{ (*_proc)(*this, e, _closure); }

    //  Private Class Methods

    static void init();
    static void read_handler(int, void *);

};

class FileMonitor : public FAMonitor {

public:

    FileMonitor(const char *path, ChangeProc proc, void *closure)
				     : FAMonitor(path, FILE, proc, closure) { }

};

class DirectoryMonitor : public FAMonitor {

public:

    DirectoryMonitor(const char *path, ChangeProc proc, void *closure)
				      : FAMonitor(path, DIR, proc, closure) { }
    
};

#endif /* !FAMonitor_included */
