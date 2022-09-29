#ifndef Cred_included
#define Cred_included

#include <sys/param.h>
#include <sys/types.h>

#include "Boolean.h"

//  Cred is short for Credentials, which is what NFS calls the
//  structure that holds the user's uids and gids.
//
//  A user of a Cred can get its uid and gid, and get an ASCII string
//  for its group list.  A user can also pass the message
//  become_user() which will change the process's effective uid and
//  gid and group list to match the Cred's.  If the new IDs are the
//  same as the current IDs, become_user() doesn't do any system
//  calls.
//
//  The Cred itself is simply a pointer to the Implementation.  The
//  Implementation is reference counted, so when the last Cred
//  pointing to one is destroyed, the Implementation is destroyed too.
//
//  Implementations are shared.  There is currently a linked list of
//  all Implementations, and that list is searched whenever a new Cred
//  is created.  A faster lookup method would be good...


class Cred {

public:

    enum { GROUP_STR_MAX = 12 * NGROUPS_UMAX };

    Cred(uid_t, unsigned int ngroups, const gid_t *);
    Cred(const Cred& that)		: p(that.p) { p->refcount++; }
    Cred& operator = (const Cred& that);
    ~Cred()				{ if (!--p->refcount) drop(p); }

    uid_t uid() const			{ return p->uid(); }
    uid_t gid() const			{ return p->gid(); }
    void printgroups(char *s, unsigned n = 1) const
					{ p->printgroups(s, n); }

    void become_user() const		{ p->become_user(); }

    static const Cred SuperUser;

private:

    class Implementation {

    public:

	Implementation(uid_t, unsigned int, const gid_t *);
	~Implementation();
	int cmp(uid_t, unsigned ngroups, const gid_t *) const;

	uid_t uid() const		{ return myuid; }
	gid_t gid() const		{ return groups[0]; }
	void printgroups(char *, unsigned);
	
	void become_user() const;

	unsigned refcount;

    private:

	uid_t myuid;
	unsigned int ngroups;
	gid_t groups[NGROUPS_UMAX];

	Boolean groups_change() const;

	static const Implementation *last;

    };

    Implementation *p;

    static Implementation **impllist;
    static unsigned nimpl, nimpl_alloc;

    static void add(Implementation *);
    static void drop(Implementation *);

};

#endif /* !Cred_included */
