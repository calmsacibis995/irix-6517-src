#ifndef FormatLibrary_included
#define FormatLibrary_included

#include "Task.H"
#include "bool.H"

class FormatDSO;

class FormatLibrary {

public:

    FormatLibrary(const char *directory);
    ~FormatLibrary();

    FormatDSO *operator [] (unsigned int);

    void flush();

private:

    enum { MAX_FORMAT_DSOS = 100 };

    //  Instance Variables

    FormatDSO *_dsos[MAX_FORMAT_DSOS];
    unsigned int _ndsos;
    Task _flush_task;

    //  Private Class Methods

    static void flush_proc(Task&, void *);
    static int compare(const void *, const void *); // qsort comparison routine

};

#endif /* !FormatLibrary_included */
