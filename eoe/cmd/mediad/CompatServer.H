#ifndef CompatServer_included
#define CompatServer_included

#include "IOHandler.H"

class DeviceMonitor;
struct emsg;
struct rmsg;

class CompatServer {

public:

    CompatServer(int);
    ~CompatServer();

private:

    ReadHandler _client_handler;
    int _sock;
    char *_exclusive_filename;
    char *_exclusive_progname;
    
    void read_message(int sock);
    bool handle_eject_msg      (const emsg&, rmsg&);
    bool handle_test_msg       (const emsg&, rmsg&);
    bool handle_terminate_msg  (const emsg&, rmsg&);
    bool handle_showmount_msg  (const emsg&, rmsg&);
    bool handle_suspend_msg    (const emsg&, rmsg&);
    bool handle_resume_msg     (const emsg&, rmsg&);
    bool handle_setloglevel_msg(const emsg&, rmsg&);

    DeviceMonitor *path_to_monitor(const char *path);

    static void client_read_proc(int, void *);

};

#endif /* !CompatServer_included */
