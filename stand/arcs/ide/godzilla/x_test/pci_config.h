#define PCI_CONFIG_VENDOR_DEVICE_ID        0x0000000
#define PCVEND                             0x0000000
#define PCI_CONFIG_COMMAND_STATUS          0x0000004
#define PCCSR                              0x0000004
#define PCI_CONFIG_REV_ID_CLASS            0x0000008
#define PCREV                              0x0000008
#define PCI_CONFIG_HEADER_LATENCY          0x000000c
#define PCHDR                              0x000000c
#define PCI_CONFIG_BASE_ADDRESS            0x0000010
#define PCBASE                             0x0000010
#define PCI_CONFIG_MISC                    0x000003c
#define PCMISC                             0x000003c

struct pci_config{
	int	vendor_dev_id;
	int	conf_cmd_stat;
	int	rev_id_class;
	int	header_latency;
	int	base_address;
	int	dummy[9];
	int	conf_misc;
	};

