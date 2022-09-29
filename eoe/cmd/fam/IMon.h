#ifndef IMon_included
#define IMon_included

#include <sys/stat.h>

#include "Boolean.h"

struct stat;

//  IMon is an object encapsulating the interface to /dev/imon.
//  There can only be one instantiation of the IMon object.
//
//  The user of this object uses express() and revoke() to
//  express/revoke interest in a file to imon.  There is also
//  a callback, the EventHandler.  When an imon event comes in,
//  the EventHandler is called.
//
//  The user of the IMon object is the Interest class.

class IMon {

public:

    enum Status { OK = 0, BAD = -1 };
    enum Event { EXEC, EXIT, CHANGE };

    typedef void (*EventHandler)(dev_t, ino_t, int event);

    IMon(EventHandler h)		{ ehandler = h; count++; }
    ~IMon();

    static Boolean is_active();

    Status express(const char *name, struct stat *stat_return);
    Status revoke(const char *name, dev_t dev, ino_t ino);

private:

    //  Class Variables

    static int imonfd;
    static unsigned count;
    static EventHandler ehandler;

    static void read_handler(int fd, void *closure);

    IMon(const IMon&);			// Do not copy
    operator = (const IMon&);		//  or assign.

};

#endif /* !IMon_included */
