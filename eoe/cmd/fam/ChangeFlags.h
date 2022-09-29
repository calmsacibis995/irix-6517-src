#ifndef ChangeFlags_included
#define ChangeFlags_included

//  ChangeFlags is a bitmask of things that may have changed in a stat
//  structure.  It's generated in Interest::do_stat() and it's
//  enclosed inside an Event, then decoded and written to a client
//  in ClientConnection::send_event().

class ChangeFlags {

    typedef unsigned char FlagWord;

public:

    enum Bit {
	DEV   = 0x1,
	INO   = 0x2,
	UID   = 0x4,
	GID   = 0x8,
	SIZE  = 0x10,
	MTIME = 0x20,
	CTIME = 0x40,
    };
    ChangeFlags()			: flags(0) { }
    ChangeFlags(const char *, const char **);

    void set(Bit b)			{ flags |= b; }
    const char *value() const;
    operator FlagWord () const		{ return flags; }

private:

    FlagWord flags;

    static char value_buffer[8];

};

#endif /* !ChangeFlags_included */
