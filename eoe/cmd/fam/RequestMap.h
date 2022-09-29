#ifndef RequestMap_included
#define RequestMap_included

#include "BTree.h"
#include "Request.h"

class ClientInterest;

//  A RequestMap 
//
//  Both TCP_Clients and ServerHosts have RequestMaps.

typedef BTree<Request, ClientInterest *> RequestMap;

#endif /* !RequestMap_included */
