#ifndef FormatIndex_included
#define FormatIndex_included

//  These manifest constants identify the known disk formats.  0-7 are
//  the "popular" formats -- the ones for which a device DSO must
//  explicitly disable support.  8-15 are the "unpopular" formats.
//  Device DSOs must explicitly enable support for those.  See
//  Device::feature_set().

enum FormatIndex {

    FMT_UNKNOWN = -1,

    FMT_RAW = 0,			// Raw bits
    FMT_EFS = 1,			// SGI Extent Filesystem
    FMT_HFS = 2,			// Apple Hierarchical Filesystem
    FMT_DOS = 3,			// Microsoft DOS filesystem
    FMT_XFS = 4,			// SGI x Filesystem

    FMT_ISO = 8,			// ISO-9660 filesystem
    FMT_CDDA = 9,			// Compact Disc Digital Audio
    FMT_AUDIO = 10,			// non-CD audio (e.g., DAT)

    MAX_FORMATS = 16			// bits in a short -- see Device.H
};

#endif /* !FormatIndex_included */
