#ifndef Event_included
#define Event_included

#include <fam.h>

#include "ChangeFlags.h"

//  An Event is a FAM event (not an imon event or a select event).  An
//  Event has a type, which corresponds to the FAMCodes types defined
//  in <fam.h>.  A Changed event also has a ChangeFlags that explains
//  what changed.

class Event {

public:

    enum Type {
	Changed     = FAMChanged,
	Deleted     = FAMDeleted,
	Executing   = FAMStartExecuting,
	Exited      = FAMStopExecuting,
	Created     = FAMCreated,
	Moved       = FAMMoved,
	Acknowledge = FAMAcknowledge,
	Exists      = FAMExists,
	EndExist    = FAMEndExist,
    };

    Event(Type n = Type(0))		: which(n) { }
    Event(char);
    Event(const ChangeFlags& f)		: which(Changed), flags(f) { }

    operator int ()       const		{ return which; }
    const char *changes() const		{ return flags.value(); }
    const char *name()    const;
    char code()           const;

private:

    char which;
    ChangeFlags flags;

};

#endif /* !Event_included */
