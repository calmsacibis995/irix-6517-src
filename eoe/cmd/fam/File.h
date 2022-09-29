#ifndef File_included
#define File_included

#include "ClientInterest.h"

//  File is the concrete type for a file in which a client has
//  expressed an interest.  It doesn't add any new functionality over
//  ClientInterest; it's just concrete.  (ClientInterest is abstract.)

class File : public ClientInterest {

public:

    File(const char *name, Client *, Request, const Cred&);
    Type type() const;

};

#endif /* !File_included */
