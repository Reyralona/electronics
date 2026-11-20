#include "eeprom.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    Eeprom memory;
    eeprom_init(&memory, NULL);
    EepromPins pins = {2, 0x5A, 1, 1, 0, 1, 1, 1};
    eeprom_update(&memory, 0, pins);
    /* Power-on inhibit: an early pulse cannot write. */
    pins.we = 0;
    eeprom_update(&memory, .001, pins);
    pins.we = 1;
    eeprom_update(&memory, .001001, pins);
    assert(!memory.busy && memory.image.data[2] == 0xFF);
    pins.we = 0;
    eeprom_update(&memory, .006, pins);
    /* The address is latched on entry; data is captured on exit. */
    pins.address = 3;
    pins.data = 0xA5;
    eeprom_update(&memory, .0060001, pins);
    pins.we = 1;
    eeprom_update(&memory, .006001, pins);
    assert(memory.busy && memory.image.data[2] == 0xFF);
    unsigned byte;
    pins.oe = 0;
    eeprom_update(&memory, .006002, pins);
    assert(!eeprom_output(&memory, &byte));
    eeprom_update(&memory, .0060021, pins);
    assert(eeprom_output(&memory, &byte) && (byte & 128) == 0);
    eeprom_update(&memory, .007002, pins);
    assert(memory.dirty && memory.image.data[2] == 0xA5 && memory.image.data[3] == 0xFF);
    pins.address = 2;
    eeprom_update(&memory, .008, pins);
    assert(!eeprom_output(&memory, &byte));
    eeprom_update(&memory, .008001, pins);
    assert(eeprom_output(&memory, &byte) && byte == 0xA5);
    /* OE low inhibits writes. */
    pins.we = 0;
    eeprom_update(&memory, .009, pins);
    pins.we = 1;
    eeprom_update(&memory, .009001, pins);
    assert(!memory.busy);
    /* CE can end a write too; replacing a byte can set zero bits back to one. */
    pins.oe = 1;
    pins.data = 0xFF;
    eeprom_update(&memory, .010, pins);
    pins.we = 0;
    eeprom_update(&memory, .011, pins);
    pins.ce = 1;
    eeprom_update(&memory, .011001, pins);
    eeprom_update(&memory, .012002, pins);
    assert(memory.image.data[2] == 0xFF);
    /* A too short pulse cannot write, and power loss cancels an unfinished cycle. */
    pins.ce = 0;
    pins.we = 1;
    pins.data = 0x12;
    eeprom_update(&memory, .013, pins);
    pins.we = 0;
    eeprom_update(&memory, .014, pins);
    pins.we = 1;
    eeprom_update(&memory, .01400001, pins);
    assert(!memory.busy);
    pins.we = 0;
    eeprom_update(&memory, .015, pins);
    pins.we = 1;
    eeprom_update(&memory, .015001, pins);
    assert(memory.busy);
    pins.powered = 0;
    eeprom_update(&memory, .015002, pins);
    assert(!memory.busy && memory.image.data[2] == 0xFF);
    pins.powered = 1;
    eeprom_update(&memory, .016, pins);
    assert(memory.image.data[2] == 0xFF);
    puts("EEPROM: edges, address/data latch, polling, overwrite, inhibit, pulse width and power loss passed.");
    return 0;
}
