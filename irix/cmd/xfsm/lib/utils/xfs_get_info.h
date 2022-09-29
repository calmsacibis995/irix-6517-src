#define SCSI_NUMBER     0                       /* Internal number for scsi */
#if !defined(STANDALONE) && defined(SCSI)
/* smfd floppy under unix only, and only if SCSI defined. */
#define SMFD_NUMBER     3                       /* Internal number for smfd */
#define RAID_NUMBER     5                       /* Internal number for RAID */
#endif

struct defect_list {
        struct defect_header defect_hdr;
        struct defect_entry  defect[8190];
};

