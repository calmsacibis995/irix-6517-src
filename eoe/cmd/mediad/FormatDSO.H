#ifndef FormatDSO_included
#define FormatDSO_included

#include "DSO.H"

class Device;

class FormatDSO : public DSO {

public:

    FormatDSO(const char *path)		: DSO(path) { }

    void inspect(Device&);

    void flush()			{ unload(); }

};

#endif /* !FormatDSO_included */
