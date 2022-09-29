#ifndef CompatClient_included
#define CompatClient_included

#include <mntent.h>
#include <mediad.h>

#include "bool.H"

class CompatClient {

public:

    CompatClient();

    int status()			{ return reply().error; }
    void verbose(bool yesno);
    const rmsg& reply();
    const char *error_msg()		{ return message(status()); }

    void send_test();
    void send_eject(const char *path, int, int, int);
    void send_kill();
    void send_show_mpoint(const char *fsname);
    void send_start_entry(const char *fsname);
    void send_query_dev(const char *fsname);
    void send_stop_entry(const char *fsname);
    void send_set_log_level(int);

    static const char *message(int code);
private:

    bool _sent;
    bool _verbose;
    emsg _msg;
    rmsg _reply;

    CompatClient(const CompatClient&);	// Do not copy
    void operator = (const CompatClient&);//  or assign.

    void send_mediad_sockmsg(int);

    static void sigalrm_handler();

};

#endif /* !CompatClient_included */
