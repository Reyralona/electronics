#include "eeprom.h"
#include <math.h>
#include <string.h>

void eeprom_init(Eeprom *memory, const RomImage *image)
{
    memset(memory, 0, sizeof(*memory));
    if (image)
        memory->image = *image;
    else
        memset(memory->image.data, ROM_IMAGE_ERASED, sizeof(memory->image.data));
    memory->previous.ce = memory->previous.oe = memory->previous.we = 1;
}

void eeprom_update(Eeprom *memory, double time, EepromPins pins)
{
    if (!memory || !isfinite(time) || time < memory->now)
        return;
    EepromPins old = memory->previous;
    memory->now = time;
    if (!memory->initialized || pins.address != old.address || pins.address_known != old.address_known)
        memory->address_changed = time;
    if (!memory->initialized || pins.data != old.data || pins.data_known != old.data_known)
        memory->data_changed = time;
    if (!memory->initialized || pins.oe != old.oe)
        memory->oe_changed = time;
    if (!memory->initialized || pins.ce != old.ce)
        memory->ce_changed = time;
    if (pins.powered && (!old.powered || !memory->initialized))
        memory->power_ready = time + EEPROM_POWER_ON_SECONDS;
    if (!pins.powered)
    {
        memory->armed = 0;
        memory->busy = 0;
    }
    if (memory->busy && time < memory->data_hold_until &&
        (pins.data != old.data || !pins.data_known || pins.oe != 1))
        memory->busy = 0;
    if (memory->busy && time >= memory->busy_until)
    {
        if (memory->image.data[memory->write_address] != memory->write_data)
        {
            memory->image.data[memory->write_address] = (unsigned char)memory->write_data;
            memory->dirty = 1;
        }
        memory->busy = 0;
    }
    int active = pins.ce == 0 && pins.we == 0;
    int was_active = old.ce == 0 && old.we == 0;
    if (active && !was_active)
    {
        memory->armed = !memory->busy && pins.powered && time >= memory->power_ready &&
                        pins.oe == 1 && pins.address_known && pins.address < ROM_IMAGE_BYTES &&
                        time >= memory->address_changed + EEPROM_ADDRESS_SETUP_SECONDS &&
                        time >= memory->oe_changed + EEPROM_ADDRESS_SETUP_SECONDS;
        memory->write_address = pins.address;
        memory->pulse_started = time;
    }
    if (memory->armed && pins.oe != 1)
        memory->armed = 0;
    if (was_active && !active)
    {
        if (memory->armed && pins.powered && pins.oe == 1 && pins.ce >= 0 && pins.we >= 0 &&
            pins.data_known && pins.data <= 255 &&
            time - memory->pulse_started >= EEPROM_WRITE_PULSE_MIN_SECONDS &&
            time - memory->data_changed >= EEPROM_DATA_SETUP_SECONDS)
        {
            memory->write_data = pins.data;
            memory->busy_until = time + EEPROM_WRITE_SECONDS;
            memory->data_hold_until = time + EEPROM_DATA_HOLD_SECONDS;
            memory->busy = 1;
        }
        memory->armed = 0;
    }
    memory->previous = pins;
    memory->initialized = 1;
}

int eeprom_output(const Eeprom *memory, unsigned *byte)
{
    const EepromPins *pins = &memory->previous;
    if (!memory->initialized || !pins->powered || pins->ce != 0 || pins->oe != 0 || pins->we != 1 ||
        !pins->address_known || pins->address >= ROM_IMAGE_BYTES ||
        memory->now < memory->address_changed + EEPROM_ACCESS_SECONDS ||
        memory->now < memory->ce_changed + EEPROM_ACCESS_SECONDS ||
        memory->now < memory->oe_changed + EEPROM_OE_ACCESS_SECONDS)
        return 0;
    *byte = memory->busy ? memory->write_data ^ 0x80u : memory->image.data[pins->address];
    return 1;
}

double eeprom_next_event(const Eeprom *memory)
{
    double next = INFINITY;
    double access = memory->address_changed + EEPROM_ACCESS_SECONDS;
    access = fmax(access, memory->ce_changed + EEPROM_ACCESS_SECONDS);
    access = fmax(access, memory->oe_changed + EEPROM_OE_ACCESS_SECONDS);
    if (access > memory->now)
        next = access;
    if (memory->busy && memory->busy_until > memory->now && memory->busy_until < next)
        next = memory->busy_until;
    return next;
}
