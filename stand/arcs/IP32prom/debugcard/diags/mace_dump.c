#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/mace_ec.h>


#define TIMEOUT 200

typedef struct reg_table{
        char		*regname;
        long long	regaddr;
}reg_table_t;

static reg_table_t mace_regs[] = {
	{"ISA_RINGBASE", PHYS_TO_K1(ISA_RINGBASE)},
	{"ISA_FLASH_NIC_REG", PHYS_TO_K1(ISA_FLASH_NIC_REG)},
	{"ISA_INT_STS_REG", PHYS_TO_K1(ISA_INT_STS_REG)},
	{"ISA_INT_MSK_REG", PHYS_TO_K1(ISA_INT_MSK_REG)},
	{"PCI_ERROR_ADDR/FLAG",	PHYS_TO_K1(PCI_ERROR_ADDR)},
	{"PCI_CONTROL",	PHYS_TO_K1(PCI_CONTROL)},
	{"PCI_CONFIG_ADDR/DATA",	PHYS_TO_K1(PCI_CONFIG_ADDR)},
	};



int
mace_dump_regs(void)
{
	int i;
	reg_table_t *mr;

	for(mr = mace_regs; mr->regname; mr++)
		printf("%s	%x\n", \
		mr->regname, \
		READ_REG64(mr->regaddr,long long));


}
