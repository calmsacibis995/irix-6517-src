/* gpdata.c -- data for getpath(), and pathname parsing.
 */

#ident "$Revision: 1.13 $"

#include <arcs/hinv.h>

struct cfgdata ParseData[] = {
	"scsi",		4,	SCSIAdapter,
	"disk",		4,	DiskController,
	"rdisk",	5,	DiskPeripheral,
	"serial",	6,	SerialController,
	"video",	5,	DisplayController,
	"keyboard",	3,	KeyboardPeripheral,
	"pointer",	5,	PointerPeripheral,
	"network",	3,	NetworkController,
	"network",	3,	NetworkPeripheral,
	"multi",	4,	MultiFunctionAdapter,
	"tape",		4,	TapeController,
	"tape",		4,	TapePeripheral,
	/* note: above ordered to reduce search length for common paths */
	"line",		4,	LinePeripheral,
	"monitor",	7,	MonitorPeripheral,
	"eisa",		4,	EISAAdapter,
	"xio",		3,	XTalkAdapter,
	"pci",		3,	PCIAdapter,
	"gio",		3,	GIOAdapter,
	"tpu",		3,	TPUAdapter,
	"cdrom",	2,	CDROMController,
	"disk",		4,	CDROMController, 	/* allow disk for cd */
	"worm",		4,	WORMController,
	"par",		3,	ParallelController,
	"pointer",	5,	PointerController,
	"key",		3,	KeyboardController,
	"audio",	5,	AudioController,
	"fdisk",	2,	FloppyDiskPeripheral,
	"modem",	5,	ModemPeripheral,
	"printer",	5,	PrinterPeripheral,
	"dti",		3,	DTIAdapter,
	"term",		4,	TerminalPeripheral,
	"other",	5,	OtherController,
	"other",	4,	OtherPeripheral,
	0,		0,	Anonymous
};
