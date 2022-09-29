#include "FormatRaw.H"

#include "Device.H"
#include "Partition.H"
#include "PartitionAddress.H"

void FormatRaw::inspect(Device& device)
{
    int secsize = device.sector_size(FMT_RAW);
    __uint64_t nsectors = device.capacity();
    PartitionAddress address(device.address(),
			     FMT_RAW,
			     PartitionAddress::WholeDisk);
    (void) Partition::create(address, &device, secsize, 0, nsectors, "raw");
}
