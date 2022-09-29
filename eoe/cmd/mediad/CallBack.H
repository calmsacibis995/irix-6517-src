#ifndef CallBack_included
#define CallBack_included

class Event;

class CallBack {

public:

    typedef void (*CallbackProc)(const Event&, void *closure);

    static void add(CallbackProc, void *);
    static void remove(CallbackProc, void *);

    static void activate(const Event&);

private:

    struct Pair {
	void (*proc)(const Event&, void *closure);
	void *closure;
    };

    static unsigned int ncallbacks;
    static unsigned int ncall_alloc;
    static Pair *callbacks;
    
};

#endif /* !CallBack_included */
