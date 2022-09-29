#ifndef CompatListener_included
#define CompatListener_included

#include "IOHandler.H"
#include "bool.H"

class CompatListener {

    typedef unsigned long u_long;

public:

    static const char MEDIAD_SOCKNAME[];

    CompatListener(const char *sock_path);
    ~CompatListener();

private:

//    char *_sock_path;
    ReadHandler _rendezvous_handler;

    static void accept_client(int fd, void *closure);

};

#endif /* !CompatListener_included */
