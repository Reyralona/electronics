#include "netlist_protection.h"
#include <stdlib.h>
#include <string.h>

void netlist_protection_reset(NetlistProtection *protection)
{
    protection->current_count = 0;
    protection->voltage_count = 0;
    protection->failed = 0;
}

void netlist_protection_free(NetlistProtection *protection)
{
    free(protection->current);
    free(protection->voltage);
    memset(protection, 0, sizeof(*protection));
}

/* Grows a term array; on allocation failure the protection is marked failed and the term dropped. */
static void *grow(NetlistProtection *protection, void *items, int count, int *capacity, size_t size)
{
    if (count < *capacity)
        return items;
    int wanted = *capacity ? *capacity * 2 : 64;
    void *larger = realloc(items, (size_t)wanted * size);
    if (!larger)
    {
        protection->failed = 1;
        return NULL;
    }
    *capacity = wanted;
    return larger;
}

void netlist_protection_add_current(NetlistProtection *protection, ProtectionCurrent term)
{
    if (!protection)
        return;
    ProtectionCurrent *items = grow(protection, protection->current, protection->current_count,
                                    &protection->current_capacity, sizeof(term));
    if (!items)
        return;
    protection->current = items;
    items[protection->current_count++] = term;
}

void netlist_protection_add_voltage(NetlistProtection *protection, ProtectionVoltage term)
{
    if (!protection)
        return;
    ProtectionVoltage *items = grow(protection, protection->voltage, protection->voltage_count,
                                    &protection->voltage_capacity, sizeof(term));
    if (!items)
        return;
    protection->voltage = items;
    items[protection->voltage_count++] = term;
}

void netlist_protection_output(NetlistProtection *protection, int part, int pin, int vcc, int gnd, int node,
                               const char *high, const char *low_plus, const char *low_minus, double limit_amps)
{
    if (!protection)
        return;
    ProtectionCurrent term = {.part = part, .pin = pin, .limit_amps = limit_amps};
    snprintf(term.vcc, sizeof(term.vcc), "%d", vcc);
    snprintf(term.gnd, sizeof(term.gnd), "%d", gnd);
    snprintf(term.node, sizeof(term.node), "%d", node);
    snprintf(term.high, sizeof(term.high), "%s", high);
    snprintf(term.low_plus, sizeof(term.low_plus), "%s", low_plus);
    snprintf(term.low_minus, sizeof(term.low_minus), "%s", low_minus);
    netlist_protection_add_current(protection, term);
}

void netlist_protection_supply(NetlistProtection *protection, int part, const char *supply, double absolute_max,
                               double recommended_max)
{
    if (!protection)
        return;
    ProtectionVoltage term = {.part = part, .absolute_max = absolute_max, .recommended_max = recommended_max};
    snprintf(term.supply, sizeof(term.supply), "%s", supply);
    netlist_protection_add_voltage(protection, term);
}
