#ifndef FSDTab_included
#define FSDTab_included

class Event;

class FSDTab {

public:

    FSDTab();
    ~FSDTab();

private:

    FSDTab(const FSDTab&);		// Do not copy
    operator = (const FSDTab&);		//  or assign.

    static const char fsdtab_path[];

    void update();

    static void event_proc(const Event&, void *);

};

#endif /* !FSDTab_included */
