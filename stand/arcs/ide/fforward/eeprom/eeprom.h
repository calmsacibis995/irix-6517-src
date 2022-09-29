#ident	"fforward/eeprom/eeprom.h:  $Revision: 1.3 $"

#include "uif.h"

#define	BAD_EEPROM		0xffff
#define	MAX_EEPROM_SIZE		128

unsigned short eeprom_read (unsigned short);
int eeprom_write (unsigned short, unsigned short);
unsigned short read_protect_register (void);
void size_eeprom (void);
