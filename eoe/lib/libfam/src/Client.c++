#include "Client.h"
Client::Client()
{
	Slist=0;
}

Client::~Client()
{
    for (ServerEntry *p = Slist, *next = NULL; p; p = next)
    {   next = p->next;
	close(p->sock);
	delete p;
    }
}
