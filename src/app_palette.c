#include "app_palette.h"
#include "component_registry.h"
#include "ui_text.h"
#include <ctype.h>
#include <string.h>

/* The saved-chip library keeps page 2; the remaining pages contain each built-in part once. */
static const PartType basic[] = {PART_RESISTOR, PART_CAPACITOR, PART_INDUCTOR, PART_POTENTIOMETER,
    PART_DIODE, PART_NPN, PART_PNP, PART_SWITCH, PART_BUTTON, PART_DPST, PART_DIP_SWITCH};
static const PartType digital[] = {PART_AND, PART_NAND, PART_OR, PART_XOR, PART_NOT, PART_NOR, PART_XNOR,
    PART_DFF, PART_CLOCK, PART_74HC00, PART_74HC08, PART_74HC32, PART_74HC86, PART_74HC04,
    PART_74HC02, PART_74HC10, PART_74HC11, PART_74HC14};
/* Logic ICs by function: flip-flops, counters, registers, arithmetic, bus. */
static const PartType logic[] = {PART_74HC74, PART_74HC273, PART_74HC374, PART_74HC373, PART_74HC161, PART_74HC163,
    PART_74HC193, PART_74HC173, PART_74HC138, PART_74HC139, PART_74HC151, PART_74HC157,
    PART_74HC4511, PART_74HC283, PART_74HC245};
static const PartType memory[] = {PART_AT28C16};
static const PartType linear[] = {PART_NE555, PART_LM358, PART_LM393, PART_LM7805, PART_ULN2003, PART_L293D};
static const PartType sensors[] = {PART_LDR, PART_FLEX, PART_FORCE, PART_TILT, PART_PHOTODIODE,
    PART_AMBIENT_LIGHT, PART_TEMPERATURE, PART_SOIL, PART_PIR, PART_GAS, PART_IR_OBSTACLE, PART_DISTANCE};
static const PartType outputs[] = {PART_LED, PART_RGB_LED, PART_LAMP, PART_SEVEN_SEGMENT, PART_LCD_I2C,
    PART_RELAY, PART_DC_MOTOR, PART_VIBRATION_MOTOR, PART_ENCODER_MOTOR, PART_SERVO, PART_PIEZO};
static const PartType instruments[] = {PART_MULTIMETER, PART_OSCILLOSCOPE, PART_FUNCTION_GENERATOR};
static const PartType power[] = {PART_SOURCE, PART_SOURCE_AC, PART_BATTERY_9V, PART_BATTERY_AA, PART_BATTERY_COIN};
static const PartType boards[] = {PART_ARDUINO_UNO, PART_ESP32, PART_ATTINY85};
static const PartType workbench[] = {PART_BREADBOARD, PART_BREADBOARD_WIDE, PART_JUNCTION, PART_NOTE};

#define COUNT(list) ((int)(sizeof(list) / sizeof(list[0])))

int is_gate(PartType type)
{
    return component_has_flag(type, COMPONENT_GATE_SYMBOL);
}

int palette_count(int page)
{
    static const int counts[] = {COUNT(basic),  COUNT(digital), 8,           COUNT(sensors),
                                 COUNT(outputs), COUNT(instruments), COUNT(power), COUNT(boards),
                                 COUNT(workbench), COUNT(logic), COUNT(memory), COUNT(linear)};
    return page >= 0 && page < PALETTE_PAGES ? counts[page] : 0;
}

PartType palette_part(int page, int item)
{
    static const PartType *lists[] = {basic, digital, NULL, sensors, outputs, instruments, power, boards, workbench,
                                      logic, memory, linear};
    if (page < 0 || page >= PALETTE_PAGES || !lists[page] || item < 0 || item >= palette_count(page))
        return PART_RESISTOR;
    return lists[page][item];
}

const char *palette_page_name(int page)
{
    static const TextId names[PALETTE_PAGES] = {TXT_UI_PAGE_BASICS,  TXT_UI_PAGE_DIGITAL, TXT_UI_PAGE_CHIPS,
                                                TXT_UI_PAGE_SENSORS, TXT_UI_PAGE_OUTPUTS, TXT_UI_PAGE_METERS,
        TXT_UI_PAGE_POWER, TXT_UI_PAGE_BOARDS, TXT_UI_PAGE_WORKBENCH, TXT_UI_PAGE_LOGIC, TXT_UI_PAGE_MEMORY,
        TXT_UI_PAGE_LINEAR};
    return page >= 0 && page < PALETTE_PAGES ? tr(names[page]) : "";
}

const char *part_name(PartType type)
{
    const ComponentInfo *info = component_info(type);
    return info ? info->display_name : tr(TXT_UI_UNKNOWN_PART);
}

/* shortcut: folding covers ASCII and Latin-1; extend when the catalog gains other scripts. */
static unsigned search_character(const unsigned char **text)
{
    unsigned value = *(*text)++;
    if (value == 0xc3 && **text >= 0x80 && **text <= 0xbf)
        value = 0xc0 + (*(*text)++ - 0x80);
    if (value >= 0xc0 && value <= 0xde && value != 0xd7)
        value += 32;
    if (value >= 0xe0 && value <= 0xe5)
        return 'a';
    if (value >= 0xe8 && value <= 0xeb)
        return 'e';
    if (value >= 0xec && value <= 0xef)
        return 'i';
    if (value >= 0xf2 && value <= 0xf6)
        return 'o';
    if (value >= 0xf9 && value <= 0xfc)
        return 'u';
    if (value == 0xe7)
        return 'c';
    return value < 128 ? (unsigned)tolower((int)value) : value;
}

static int name_matches(const char *name, const char *query)
{
    for (; *name; ++name)
    {
        const unsigned char *a = (const unsigned char *)name;
        const unsigned char *b = (const unsigned char *)query;
        while (*a && *b)
        {
            if (search_character(&a) != search_character(&b))
                break;
            if (!*b)
                return 1;
        }
        if (!*query)
            return 1;
    }
    return !*query;
}

static int filtered_part(const char *query, int wanted, PartType *found)
{
    int count = 0;
    for (size_t type = 0; type < component_registry_count(); ++type)
    {
        const ComponentInfo *info = component_info((PartType)type);
        if (!info || (!name_matches(info->display_name, query) && !name_matches(info->id, query)))
            continue;
        if (count == wanted && found)
            *found = (PartType)type;
        ++count;
    }
    return count;
}

int palette_filtered_count(int page, const char *query)
{
    return *query ? filtered_part(query, -1, NULL) : palette_count(page);
}

PartType palette_filtered_part(int page, const char *query, int item)
{
    PartType found = PART_RESISTOR;
    if (!*query)
        return palette_part(page, item);
    filtered_part(query, item, &found);
    return found;
}
