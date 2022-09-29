
/* NVRAM utility header files.
 */
int size_nvram(void);
void nvram_cs_on(void);
void nvram_cs_off(void);
void nvram_cmd(unsigned int, unsigned int, int);
int nvram_hold(void);

/* NVRAM tests used from IDE.
 */
int eeprom_address(void);
int eeprom_data(void);
int eeprom_id(void);
int eeprom_pins(void);
