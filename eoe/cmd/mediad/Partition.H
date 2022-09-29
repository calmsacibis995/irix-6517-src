#ifndef Partition_included
#define Partition_included

#include <sys/types.h>

#include "Enumerable.H"
#include "PartitionAddress.H"

class Device;

class Partition : private Enumerable {

    typedef __uint64_t u64;

public:

    static Partition *create(const PartitionAddress&,
			     Device *,
			     unsigned int secsize,
			     u64 sec0,
			     u64 nsec,
			     const char *fmtname);
    ~Partition();

    const PartitionAddress& address() const	{ return _address; }
    Device *device() const			{ return _device; }
    unsigned int sector_size() const		{ return _secsize; }
    u64 start_sector() const			{ return _sec0; }
    u64 n_sectors() const			{ return _nsec; }
    const char *format_name() const		{ return _fmtname; }

    //  Partition enumeration

    ENUMERATION_METHODS(Partition, partitions);

private:

    //  Instance Variables

    PartitionAddress _address;
    Device *_device;
    unsigned int _secsize;
    u64 _sec0;
    u64 _nsec;
    char *_fmtname;

    //  Class Variable

    static Enumerable::Set partitions;

    Partition(const PartitionAddress&,
	      Device *,
	      unsigned int secsize,
	      u64 sec0,
	      u64 nsec,
	      const char *_fmtname);

};

#endif /* !Partition_included */
