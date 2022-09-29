#include "Cred.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "Log.h"

static gid_t SuperUser_groups[1] = { 0 };
const Cred Cred::SuperUser(0, 1, SuperUser_groups);
const Cred::Implementation *Cred::Implementation::last = NULL;
Cred::Implementation **Cred::impllist;
unsigned Cred::nimpl;
unsigned Cred::nimpl_alloc;

Cred::Cred(uid_t u, unsigned int ng, const gid_t *gs)
{
    assert(ng <= NGROUPS_UMAX);

    for (Implementation **pp = impllist, **end = pp + nimpl; pp < end; pp++)
	if ((*pp)->cmp(u, ng, gs) == 0)
	{   (*pp)->refcount++;
	    p = *pp;
	    return;
	}

    p = new Implementation(u, ng, gs);
    add(p);
}

Cred&
Cred::operator = (const Cred& that)
{
    if (this != &that)
    {   if (!--p->refcount)
	    drop(p);
	p = that.p;
	p->refcount++;
    }
    return *this;
}

void
Cred::add(Implementation *np)
{
    if (nimpl >= nimpl_alloc)
    {   nimpl_alloc = nimpl_alloc * 3 / 2 + 3;
	Implementation **nl = new Implementation *[nimpl_alloc];
	for (int i = 0; i < nimpl; i++)
	    nl[i] = impllist[i];
	delete [] impllist;
	impllist = nl;
    }
    impllist[nimpl++] = np;
}

void
Cred::drop(Implementation *dp)
{
    assert(!dp->refcount);
    for (Implementation **pp = impllist, **end = pp + nimpl; pp < end; pp++)
	if (*pp == dp)
	{   *pp = *--end;
	    assert(nimpl);
	    --nimpl;
	    break;
	}
    // char buf[GROUP_STR_MAX];
    // dp->printgroups(buf, 0);
    // Log::debug("destroyed cred u=%d g=%s", dp->uid(), buf);
    delete dp;

    if (!nimpl)
    {   delete impllist;
	impllist = NULL;
	nimpl_alloc = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////

Cred::Implementation::Implementation(uid_t u, unsigned int ng, const gid_t *gs)
: refcount(1), myuid(u), ngroups(ng)
{
    for (int i = 0; i < ng; i++)
	groups[i] = gs[i];
    // char buf[GROUP_STR_MAX];
    // printgroups(buf, 0);
    // Log::debug("created cred u=%d g=%s", uid(), buf);
}

Cred::Implementation::~Implementation()
{
    if (this == last)
	SuperUser.become_user();
}

int
Cred::Implementation::cmp(uid_t u, unsigned ng, const gid_t *gs) const
{
    if (u != myuid)
	return u - myuid;
    if (ng != ngroups)
	return ng - ngroups;
    for (int i = 0; i < ng; i++)
	if (gs[i] != groups[i])
	    return gs[i] - groups[i];
    return 0;
}

Boolean
Cred::Implementation::groups_change() const
{
    for (int i = 0; i < ngroups; i++)
	if (groups[i] != last->groups[i])
	    return true;
    return false;
}

void
Cred::Implementation::printgroups(char *p, unsigned nskip)
{
    assert(p);
    if (ngroups <= nskip)
	*p = '\0';
    else
    {   sprintf(p, "%d", ngroups - nskip);
	p += strlen(p);
	for (int i = nskip; i < ngroups; i++)
	{   sprintf(p, " %d", groups[i]);
	    p += strlen(p);
	}
    }
}

void
Cred::Implementation::become_user() const
{
    if (this == last)
	return;

    uid_t current_uid = last ? last->myuid : 0;
    if (!last || ngroups != last->ngroups || groups_change())
    {
	if (current_uid)
	{
	    /* Temporarily become root */

	    if (setreuid(-1, 0) != 0)
	    {   Log::perror("failed to set 0 uid");
		exit(1);
	    }
	    else
		current_uid = 0;
	}
	if (setgroups(ngroups, groups) != 0)
	{   Log::perror("failed to set groups");
	    exit(1);
	}
    }
    if (current_uid != myuid)
    {
	if (current_uid)
	    (void) setreuid(-1, 0);
	if (myuid && setreuid(-1, myuid))
	{   Log::perror("failed to set uid %d", myuid);
	    exit(1);
	}
    }
    last = this;
}
