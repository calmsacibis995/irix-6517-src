/* cfgdata.h -- vendor specific configuration data a few components.
 */

#ifndef _ARCS_CFGDATA_H
#define _ARCS_CFGDATA_H

#ident "$Revision: 1.8 $"

#include <arcs/types.h>

#define CFGHDR			\
	USHORT Version;		\
	USHORT Revision;	\
	char *Type;		\
	char *Vendor;		\
	char *ProductName;	\
	char *SerialNumber

/* generic (minimal) header */
struct cfgdatahdr {
	CFGHDR;
};

/* network */
typedef UCHAR MULTICAST[16];
struct net_data {
#define NET_TYPE			"net"
	CFGHDR;
	UCHAR	NetType[16];		/* either 802.3, 802.4, 802.5,
					   Ethernet, FDDI, or ARCNET. */
	ULONG	NetMaxlength;		/* max packet size (MTU) */
	ULONG	NetAddressSize;		/* size of NetAddres in bytes */
	UCHAR	NetAddress[16];		/* network hardware address */
	ULONG	NetMulticastCount;	/* entries in NetMulticast */
	MULTICAST *NetMulticast;	/* multicast address set on hardware */
	UCHAR	NetFunctional[6];	/* functional address */
};

/* DiskPeripheral */
struct disk_data {
	CFGHDR;
#define DISK_TYPE			"disk"
	ULONG	MaxCylinders;		/* maximum number of cylinders */
	ULONG	MaxHeads;		/* maximum number of heads */
	ULONG	StartCylinder;		/* starting cylinder for pre-comp */
	ULONG	MaxECC;			/* maximum ECC burst */
	ULONG	LZone;			/* landing zone */
	ULONG	Sectors;		/* sectors per track */
	ULONG	Bytes;			/* number of bytes per sector */
	ULONG	BlockSize;		/* number of bytes per block */ 
	ULONG	MaxBlocks;		/* maximum number of blocks */
};

/* Scsi Disk Peripheral */
struct scsidisk_data {
#define SCSIDISK_TYPE			"scsidisk"
	CFGHDR;
	ULONG BlockSize;
	ULONG MaxBlocks;
};

/* FloppyDiskPeripheral */
struct floppy_data {
#define FLOPPY_TYPE			"floppy"
	CFGHDR;
	CHAR	Size[8];		/* media size: 2, 3.5, 5.25 or 8" */
	ULONG	MaxDensity;		/* maximum Kb of unformatted media */
	ULONG	MountDensity;		/* Kb of currently mounted media */
};

/* TapePeripheral */
struct tape_data {
#define TAPE_TYPE			"tape"
	CFGHDR;
	UCHAR	TapeType[20];		/* tape type: see below */
	ULONG	Density;		/* max density supported in ?? */
	ULONG	Length;			/* max length supported in feet */
	ULONG	BlockSize;		/* block size used by peripheral */
};
/* some possible tape types:
 *	"9-Track"		1/2 inch reel-to-reel
 *	"8mm"			8mm tape
 *	"DAT"			DAT
 *	"QIC24"			1/4 inch standard
 *	"QIC150"		1/4 inch high density
 *	"Irwin 347"		40/60 Mb
 *	"Irwin 245"		40/60 Mb
 *	"Irwin 287"		80/120 Mb
 *	"Irwin 387"		80/120 Mb
 *	"Wangtek 5150EQ"	150/250 Mb
 *	"Wangtek 5525ES"	320/525 Mb
 *	"Ardat 4520"		1.3/2.0 Gb
 */

/* ModemPeripheral */
struct modem_data {
#define MODEM_TYPE			"modem"
	CFGHDR;
	ULONG	ModemType;		/* international telephone type */
	ULONG	Speed;			/* maximum bps */
};

/* KeyboardPeripheral */
struct keyboard_data {
#define KEYBOARD_TYPE			"keyboard"
	CFGHDR;
	ULONG	TypematicDelayMaximum;	/* max delay before repeat in us */
	ULONG	TypematicDelayMinimum;	/* min delay before repeat in us */
	ULONG	ResponseTime;		/* time for kbd to respond in ms */
	UCHAR	TypematicRateMaximum;	/* max number of repeats/second */
	UCHAR	TypematicRateMinimum;	/* min number of repeats/second */
	UCHAR	NumKeys;		/* for Windows */
	UCHAR	NumFunctionKeys;	/* for Windows */
	UCHAR	NumLEDs;		/* for Windows */
};

/* PointerPeripheral */
struct pointer_data {
#define POINTER_TYPE			"pointer"
	CFGHDR;
	USHORT	Resolution;		/* resolution in dpi */
	USHORT	NumButtons;		/* number of buttons */
	USHORT	ResponseTime;		/* time for ms to reponed in ms */
};

/* MonitorPeripheral */
struct monitor_data {
#define MONITOR_DATA			"monitor"
	CFGHDR;
	CHAR	MonitorType[16];
	USHORT	XResolution;		/* in pixels */
	USHORT	HDisplay;		/* in microseconds */
	USHORT	HBackporch;		/* in microseconds */
	USHORT	HFrontporch;		/* in microseconds */
	USHORT	HSync;			/* in microseconds */
	USHORT	VDisplay;		/* in horizontal lines */
	USHORT	VBackporch;		/* in horizontal lines */
	USHORT	VFrontporch;		/* in horizontal lines */
	USHORT	VSync;			/* in horizontal lines */
	USHORT	HScreenSize;		/* in millimeters */
	USHORT	VScreenSize;		/* in millimeters */
};

/* TerminalPeripheral */
struct terminal_data {
#define TERMINAL_DATA			"terminal"
	CFGHDR;
	USHORT	Columns;		/* max characters per lines */
	USHORT	Rows;			/* max rows with out scrolling */
	ULONG	Speed;			/* terminals current bps */
};

/* AudioInPeripheral */
struct audio_in_data {
#define AUDIO_IN_TYPE			"audio-in"
	CFGHDR;
	ULONG	Quantization;		/* Quantization modes */
#define A_QUANT_8L	0x0001		/* 8-bit Linear */
#define A_QUANT_8ML	0x0002		/* 8-bit Mu-Law */
#define A_QUANT_8AL	0x0004		/* 8-bit A-Law */
#define A_QUANT_12L	0x0008		/* 12-bit Linear */
#define A_QUANT_14L	0x0010		/* 14-bit Linear */
#define A_QUANT_16L	0x0020		/* 16-bit Linear */
#define A_QUANT_18L	0x0040		/* 18-bit Linear */
#define A_QUANT_20L	0x0080		/* 20-bit Linear */
#define A_QUANT_24L	0x0100		/* 24-bit Linear */
	USHORT	FreqStart;		/* lowest sampling frequency */
	USHORT	FreqStep;		/* freq resolution */
	ULONG	FreqSpanLow;		/* 64 bit representation of rate */
	ULONG	FreqSpanHigh;
	USHORT	MicGainsteps;		/* number of input levels available */
	USHORT	MicGainStepResolution;	/* db gain for each step */
	UCHAR	InputFlags;		/* input flags  */
#define A_INTMIC	0x01		/* internal mic */
#define A_EXTMIC	0x02		/* external mic */
#define A_LLINP		0x04		/* line/level input */
#define A_DIGINP	0x08		/* digital input */
#define A_STEREO	0x10		/* stereo */
	UCHAR	AuxSources;		/* number of auxilary input sources */
};

/* AudioOutPeripheral */
struct audio_out_data {
#define AUDIO_OUT_TYPE			"audio-out"
	CFGHDR;
	ULONG	Quantization;		/* see AudioInPeripheral */
	USHORT	FreqStart;		/* lowest output frequency */
	USHORT	FreqStep;		/* frequency resolution */
	ULONG	FreqSpanLow;		/* 64 bit representation of rate */
	ULONG	FreqSpanHigh;
	USHORT	VolumeSteps;		/* num of volume attenuation steps */
	USHORT	VolumeStepResolution;	/* attenuation of each step in -dbs */
	UCHAR	OutputFlags;		/* output flags */
#define	A_INTSPK	0x01
#define A_EXTSPK	0x02
#define A_LINOUT	0x04
#define A_DIGOUT	0x08
#define A_HEDOUT	0x10
};

#endif
