#include "Client.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

Client::Client(const char *name)
    : myname(name ? strcpy(new char[strlen(name) + 1], name) : NULL)
{ }

Client::~Client()
{
    delete [] myname;
}

const char *
Client::name()
{
    return myname ? myname : "unknown";
}

void
Client::name(const char *newname)
{
    delete [] myname;
    myname = newname ? strcpy(new char[strlen(newname) + 1], newname) : NULL;
}

