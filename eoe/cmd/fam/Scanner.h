#ifndef Scanner_included
#define Scanner_included

#include "Boolean.h"
#include "Request.h"

class Client;
class Event;

//  Scanner is an abstract base class for a short-lived object that
//  scans an Interest.
//
//  Why?  Well, we can potentially generate a lot of data while
//  scanning a directory, so output could get blocked in mid scan, so
//  we need something to hold the scan's context while we wait for
//  output to unblock.
//
//  Since the output flow control is managed by the Client, the Client
//  owns the Scanner.  But the Scanner is created by a ClientInterest.
//
//  Yes, it's messy.

class Scanner {

public:

    virtual ~Scanner();
    virtual Boolean done();

};

#endif /* !Scanner_included */
