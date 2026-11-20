#ifndef EEPROM_H
#define EEPROM_H
#include "rom_image.h"
#define EEPROM_ACCESS_SECONDS 150e-9
#define EEPROM_OE_ACCESS_SECONDS 70e-9
#define EEPROM_WRITE_SECONDS .001
#define EEPROM_POWER_ON_SECONDS .005
#define EEPROM_WRITE_PULSE_MIN_SECONDS 100e-9
#define EEPROM_ADDRESS_SETUP_SECONDS 10e-9
#define EEPROM_DATA_SETUP_SECONDS 50e-9
#define EEPROM_DATA_HOLD_SECONDS 10e-9

typedef struct
{
    unsigned address, data;
    int address_known, data_known;
    int ce, oe, we, powered;
} EepromPins;

typedef struct
{
    RomImage image;
    EepromPins previous;
    double now, power_ready, address_changed, data_changed, oe_changed, ce_changed;
    double pulse_started, busy_until, data_hold_until;
    unsigned write_address, write_data;
    int initialized, armed, busy, dirty;
} Eeprom;

void eeprom_init(Eeprom *memory, const RomImage *image);
void eeprom_update(Eeprom *memory, double time, EepromPins pins);
int eeprom_output(const Eeprom *memory, unsigned *byte);
double eeprom_next_event(const Eeprom *memory);
#endif
