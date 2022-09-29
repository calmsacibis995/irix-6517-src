#ifndef Format_included
#define Format_included

#include "bool.H"
#include "Partition.H"

class FormatDSO;

class Format {

public:

    Format(FormatDSO *);
    virtual ~Format();

    FormatDSO *dso()			{ return _dso; }

    virtual FormatIndex index() = 0;	// e.g., FMT_HFS
    virtual const char *name() = 0;	// e.g., "hfs"

    virtual unsigned int npartitions() const = 0;

private:

    FormatDSO *_dso;

    Format(const Format&);		// Do not copy
    void operator = (const Format&);	//  or assign.

};

#endif /* !Format_included */
