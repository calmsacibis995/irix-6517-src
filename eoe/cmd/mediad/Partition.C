#include "Partition.H"

#include "strplus.H"

Enumerable::Set Partition::partitions;

Partition::Partition(const PartitionAddress& a,
		     Device *dp,
		     unsigned int secsize,
		     u64 sec0,
		     u64 nsec,
		     const char *fmtname)
: Enumerable(partitions),
  _address(a),
  _device(dp),
  _secsize(secsize),
  _sec0(sec0),
  _nsec(nsec),
  _fmtname(NULL)
{
    if (fmtname)
	_fmtname = strdupplus(fmtname);
}

Partition::~Partition()
{
    delete [] _fmtname;
}

//////////////////////////////////////////////////////////////////////////////

Partition *
Partition::create(const PartitionAddress& paddr,
		  Device *dev,
		  unsigned int secsize,
		  u64 sec0,
		  u64 nsec,
		  const char *fmtname)
{
    return new Partition(paddr, dev, secsize, sec0, nsec, fmtname);
}
