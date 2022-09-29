#ifndef SimpleVolume_included
#define SimpleVolume_included

#include <mntent.h>

#include "Volume.H"

// A SimpleVolume is a volume of one partition.

class SimpleVolume : public Volume {

public:

    static SimpleVolume *create(Partition *, const mntent&, const char *label);
    ~SimpleVolume();

    virtual unsigned int npartitions() const;
    virtual const Partition *partition(unsigned int) const;

    virtual void use_subdir();

private:

    Partition *_part;

    SimpleVolume(Partition *, const mntent&, const char *label);
				
};

#endif /* !SimpleVolume_included */
