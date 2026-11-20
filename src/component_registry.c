#include "component_registry.h"
#include "component_devices.h"
#include "ui_text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Preserve the existing model ranges; these are data per parameter, not physical ratings. */
static const ComponentParameter voltage = {"voltage", "V", 5.0, 1e-12, 1e12};
static const ComponentParameter resistance = {"resistance", "Ω", 1000.0, 1e-12, 1e12};
static const ComponentParameter capacitance = {"capacitance", "F", 1e-6, 1e-12, 1e12};
static const ComponentParameter inductance = {"inductance", "H", .01, 1e-12, 1e12};
static const ComponentParameter frequency = {"frequency", "Hz", 1000.0, 1e-12, 1e12};

static const ComponentPinInfo pins_source[] = {{"V+", PIN_SUPPLY_POSITIVE}, {"V-", PIN_SUPPLY_RETURN}};
static const ComponentPinInfo pins_passive[] = {{"1", PIN_PASSIVE}, {"2", PIN_PASSIVE}};
static const ComponentPinInfo pins_pot[] = {{"A", PIN_PASSIVE}, {"W", PIN_PASSIVE}, {"B", PIN_PASSIVE}};
static const ComponentPinInfo pins_junction[] = {{"J", PIN_CONTACT}};
static const ComponentPinInfo pins_led[] = {{"A", PIN_ANODE}, {"K", PIN_CATHODE}};
static const ComponentPinInfo pins_gate[] = {{"A", PIN_INPUT}, {"B", PIN_INPUT}, {"Y", PIN_OUTPUT}};
static const ComponentPinInfo pins_not[] = {{"A", PIN_INPUT}, {"Y", PIN_OUTPUT}};
static const ComponentPinInfo pins_clock[] = {{"OUT", PIN_OUTPUT}, {"REF", PIN_SUPPLY_RETURN}};
static const ComponentPinInfo pins_dff[] = {{"D", PIN_INPUT}, {"CLK", PIN_CLOCK_INPUT}, {"Q", PIN_OUTPUT}};
/* TI SCLS181H, table 5-1, PDIP top view; checked 2026-11-15. */
static const ComponentPinInfo pins_74hc00[] = {
    {"1A", PIN_INPUT},  {"1B", PIN_INPUT},   {"1Y", PIN_OUTPUT}, {"2A", PIN_INPUT},           {"2B", PIN_INPUT},
    {"2Y", PIN_OUTPUT}, {"GND", PIN_GROUND}, {"3Y", PIN_OUTPUT}, {"3A", PIN_INPUT},           {"3B", PIN_INPUT},
    {"4Y", PIN_OUTPUT}, {"4A", PIN_INPUT},   {"4B", PIN_INPUT},  {"VCC", PIN_SUPPLY_POSITIVE}};
/* 0-based pin order = datasheet pin - 1; "~" marks an active-low pin. SN74HC161 checked in TI SCLS297D;
 * the others follow the standard family pinout (docs/COMPARACAO_COMPUTADOR_8_BITS.md). */
static const ComponentPinInfo pins_74hc161[] = {
    {"~CLR", PIN_INPUT}, {"CLK", PIN_CLOCK_INPUT}, {"A", PIN_INPUT},    {"B", PIN_INPUT},     {"C", PIN_INPUT},
    {"D", PIN_INPUT},    {"ENP", PIN_INPUT},       {"GND", PIN_GROUND}, {"~LOAD", PIN_INPUT}, {"ENT", PIN_INPUT},
    {"QD", PIN_OUTPUT},  {"QC", PIN_OUTPUT},       {"QB", PIN_OUTPUT},  {"QA", PIN_OUTPUT},   {"RCO", PIN_OUTPUT},
    {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentPinInfo pins_74hc173[] = {
    {"M", PIN_INPUT},   {"N", PIN_INPUT},             {"1Q", PIN_OUTPUT},  {"2Q", PIN_OUTPUT}, {"3Q", PIN_OUTPUT},
    {"4Q", PIN_OUTPUT}, {"CLK", PIN_CLOCK_INPUT},     {"GND", PIN_GROUND}, {"~G1", PIN_INPUT}, {"~G2", PIN_INPUT},
    {"4D", PIN_INPUT},  {"3D", PIN_INPUT},            {"2D", PIN_INPUT},   {"1D", PIN_INPUT},  {"CLR", PIN_INPUT},
    {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentPinInfo pins_74hc245[] = {
    {"DIR", PIN_INPUT},        {"A1", PIN_BIDIRECTIONAL}, {"A2", PIN_BIDIRECTIONAL}, {"A3", PIN_BIDIRECTIONAL},
    {"A4", PIN_BIDIRECTIONAL}, {"A5", PIN_BIDIRECTIONAL}, {"A6", PIN_BIDIRECTIONAL}, {"A7", PIN_BIDIRECTIONAL},
    {"A8", PIN_BIDIRECTIONAL}, {"GND", PIN_GROUND},       {"B8", PIN_BIDIRECTIONAL}, {"B7", PIN_BIDIRECTIONAL},
    {"B6", PIN_BIDIRECTIONAL}, {"B5", PIN_BIDIRECTIONAL}, {"B4", PIN_BIDIRECTIONAL}, {"B3", PIN_BIDIRECTIONAL},
    {"B2", PIN_BIDIRECTIONAL}, {"B1", PIN_BIDIRECTIONAL}, {"~OE", PIN_INPUT},        {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentPinInfo pins_74hc283[] = {
    {"S2", PIN_OUTPUT}, {"B2", PIN_INPUT}, {"A2", PIN_INPUT},   {"S1", PIN_OUTPUT}, {"A1", PIN_INPUT},
    {"B1", PIN_INPUT},  {"C0", PIN_INPUT}, {"GND", PIN_GROUND}, {"C4", PIN_OUTPUT}, {"S4", PIN_OUTPUT},
    {"B4", PIN_INPUT},  {"A4", PIN_INPUT}, {"S3", PIN_OUTPUT},  {"A3", PIN_INPUT},  {"B3", PIN_INPUT},
    {"VCC", PIN_SUPPLY_POSITIVE}};
/* AT28C16 shares each data terminal between programming and three-state readout. */
static const ComponentPinInfo pins_at28c16[] = {
    {"A7", PIN_INPUT},   {"A6", PIN_INPUT},   {"A5", PIN_INPUT},   {"A4", PIN_INPUT},   {"A3", PIN_INPUT},
    {"A2", PIN_INPUT},   {"A1", PIN_INPUT},   {"A0", PIN_INPUT},   {"IO0", PIN_BIDIRECTIONAL}, {"IO1", PIN_BIDIRECTIONAL},
    {"IO2", PIN_BIDIRECTIONAL}, {"GND", PIN_GROUND}, {"IO3", PIN_BIDIRECTIONAL}, {"IO4", PIN_BIDIRECTIONAL}, {"IO5", PIN_BIDIRECTIONAL},
    {"IO6", PIN_BIDIRECTIONAL}, {"IO7", PIN_BIDIRECTIONAL}, {"~CE", PIN_INPUT},  {"A10", PIN_INPUT},  {"~OE", PIN_INPUT},
    {"~WE", PIN_INPUT},  {"A9", PIN_INPUT},   {"A8", PIN_INPUT},   {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentParameter battery_9v = {"voltage", "V", 9, 1e-12, 1e12};
static const ComponentParameter battery_aa = {"voltage", "V", 1.5, 1e-12, 1e12};
static const ComponentParameter battery_coin = {"voltage", "V", 3, 1e-12, 1e12};
static const ComponentParameter flex_resistance = {"resistance", "Ω", 25000, 1, 1e9};
static const ComponentParameter force_resistance = {"resistance", "Ω", 15000, 1, 1e9};
/* Filament as a constant resistance: 250 ohm is a 5 V / 0.1 W indicator lamp; no thermal model. */
static const ComponentParameter lamp_resistance = {"resistance", "Ω", 250, 1, 1e6};
static const ComponentPinInfo pins_bjt[] = {{"E", PIN_EMITTER}, {"B", PIN_BASE}, {"C", PIN_COLLECTOR}};
static const ComponentPinInfo pins_photo_transistor[] = {{"C", PIN_COLLECTOR}, {"E", PIN_EMITTER}};
static const ComponentPinInfo pins_sensor[] = {{"V+", PIN_SUPPLY_POSITIVE}, {"OUT", PIN_OUTPUT}, {"GND", PIN_GROUND}};
static const ComponentPinInfo pins_rgb[] = {{"R", PIN_ANODE}, {"K", PIN_CATHODE}, {"G", PIN_ANODE}, {"B", PIN_ANODE}};
static const ComponentPinInfo pins_seven[] = {{"a", PIN_ANODE}, {"b", PIN_ANODE}, {"c", PIN_ANODE}, {"d", PIN_ANODE}, {"COM", PIN_CATHODE},
                                              {"e", PIN_ANODE}, {"f", PIN_ANODE}, {"g", PIN_ANODE}, {"dp", PIN_ANODE}};
static const ComponentPinInfo pins_dip[] = {{"1", PIN_CONTACT}, {"2", PIN_CONTACT}, {"3", PIN_CONTACT}, {"4", PIN_CONTACT},
                                            {"1'", PIN_CONTACT}, {"2'", PIN_CONTACT}, {"3'", PIN_CONTACT}, {"4'", PIN_CONTACT}};
static const ComponentPinInfo pins_dpst[] = {{"1A", PIN_CONTACT}, {"1B", PIN_CONTACT}, {"2A", PIN_CONTACT}, {"2B", PIN_CONTACT}};
static const ComponentPinInfo pins_relay[] = {{"B+", PIN_PASSIVE}, {"B-", PIN_PASSIVE}, {"COM", PIN_CONTACT},
                                              {NULL, PIN_CONTACT}, {NULL, PIN_CONTACT}};
static const ComponentPinInfo pins_motor[] = {{"M+", PIN_PASSIVE}, {"M-", PIN_PASSIVE}};
static const ComponentPinInfo pins_encoder[] = {{"M+", PIN_PASSIVE}, {"M-", PIN_PASSIVE}, {"VCC", PIN_SUPPLY_POSITIVE},
                                                {"GND", PIN_GROUND}, {"A", PIN_OUTPUT}, {"B", PIN_OUTPUT}};
static const ComponentPinInfo pins_servo[] = {{"GND", PIN_GROUND}, {"V+", PIN_SUPPLY_POSITIVE}, {NULL, PIN_INPUT}};
static const ComponentPinInfo pins_generator[] = {{NULL, PIN_OUTPUT}, {"COM", PIN_SUPPLY_RETURN}};
static const ComponentPinInfo pins_meter[] = {{"COM", PIN_SUPPLY_RETURN}, {"VΩ", PIN_INPUT}, {"A", PIN_INPUT}};
static const ComponentPinInfo pins_scope[] = {{"CH1", PIN_INPUT}, {"GND", PIN_GROUND}};
static const ComponentParameter generator_frequency = {"frequency", "Hz", 1000.0, 1e-3, 1e6};
static const ComponentPinInfo pins_uno[] = {
    {"D0", PIN_PASSIVE},  {"D1", PIN_PASSIVE},  {"D2", PIN_PASSIVE},  {"D3", PIN_PASSIVE},  {"D4", PIN_PASSIVE},
    {"D5", PIN_PASSIVE},  {"D6", PIN_PASSIVE},  {"D7", PIN_PASSIVE},  {"D8", PIN_PASSIVE},  {"D9", PIN_PASSIVE},
    {"D10", PIN_PASSIVE}, {"D11", PIN_PASSIVE}, {"D12", PIN_PASSIVE}, {"D13", PIN_PASSIVE}, {"A0", PIN_PASSIVE},
    {"A1", PIN_PASSIVE},  {"A2", PIN_PASSIVE},  {"A3", PIN_PASSIVE},  {"A4 SDA", PIN_PASSIVE}, {"A5 SCL", PIN_PASSIVE},
    {"5V", PIN_SUPPLY_POSITIVE}, {"3V3", PIN_SUPPLY_POSITIVE}, {"GND", PIN_GROUND}};
static const ComponentPinInfo pins_esp32[] = {
    {"2", PIN_PASSIVE},  {"4", PIN_PASSIVE},  {"5", PIN_PASSIVE},  {"12", PIN_PASSIVE}, {"13", PIN_PASSIVE},
    {"14", PIN_PASSIVE}, {"15", PIN_PASSIVE}, {"16", PIN_PASSIVE}, {"17", PIN_PASSIVE}, {"18", PIN_PASSIVE},
    {"19", PIN_PASSIVE}, {"21 SDA", PIN_PASSIVE}, {"22 SCL", PIN_PASSIVE}, {"23", PIN_PASSIVE}, {"25", PIN_PASSIVE},
    {"26", PIN_PASSIVE}, {"27", PIN_PASSIVE}, {"32", PIN_PASSIVE}, {"33", PIN_PASSIVE}, {"34", PIN_PASSIVE},
    {"3V3", PIN_SUPPLY_POSITIVE}, {"GND", PIN_GROUND}};
static const ComponentPinInfo pins_tiny[] = {{"PB5", PIN_PASSIVE}, {"PB3", PIN_PASSIVE}, {"PB4", PIN_PASSIVE},
                                             {"GND", PIN_GROUND}, {"PB0 SDA", PIN_PASSIVE}, {"PB1", PIN_PASSIVE},
                                             {"PB2 SCL", PIN_PASSIVE}, {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentPinInfo pins_lcd[] = {{"GND", PIN_GROUND}, {"VCC", PIN_SUPPLY_POSITIVE}, {"SDA", PIN_INPUT},
                                            {"SCL", PIN_INPUT}};

/* Enum indexes remain the legacy file codes; id is the durable external identity. display_name and word pin
 * names are NULL here: localized_catalog() fills them in each language. */
static const ComponentInfo components[] = {
    [PART_SOURCE] = {PART_SOURCE, "source", NULL, 2,
                     COMPONENT_ROTATES | COMPONENT_STARTS_ON | COMPONENT_CONNECTIVITY, 1, &voltage, pins_source},
    [PART_SWITCH] = {PART_SWITCH, "switch", NULL, 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL,
                     pins_passive},
    [PART_LED] = {PART_LED, "led", NULL, 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL, pins_led},
    [PART_BREADBOARD] = {PART_BREADBOARD, "breadboard", NULL, BREADBOARD_PINS, COMPONENT_CONNECTIVITY, 1, NULL,
                         NULL},
    [PART_RESISTOR] = {PART_RESISTOR, "resistor", NULL, 2, COMPONENT_ROTATES, 2, &resistance, pins_passive},
    [PART_CAPACITOR] = {PART_CAPACITOR, "capacitor", NULL, 2, COMPONENT_ROTATES, 2, &capacitance, pins_passive},
    [PART_INDUCTOR] = {PART_INDUCTOR, "inductor", NULL, 2, COMPONENT_ROTATES, 3, &inductance, pins_passive},
    [PART_AND] = {PART_AND, "and", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 11, NULL, pins_gate},
    [PART_NAND] = {PART_NAND, "nand", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_OR] = {PART_OR, "or", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_XOR] = {PART_XOR, "xor", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_NOT] = {PART_NOT, "not", NULL, 2, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_not},
    [PART_CLOCK] = {PART_CLOCK, "clock", NULL, 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 13, &frequency,
                    pins_clock},
    [PART_DFF] = {PART_DFF, "dff", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 14, NULL, pins_dff},
    [PART_NOR] = {PART_NOR, "nor", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 21, NULL, pins_gate},
    [PART_XNOR] = {PART_XNOR, "xnor", NULL, 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 21, NULL, pins_gate},
    [PART_POTENTIOMETER] = {PART_POTENTIOMETER, "potentiometer", NULL, 3, COMPONENT_ROTATES, 21, &resistance, pins_pot},
    [PART_BUTTON] = {PART_BUTTON, "button", NULL, 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 21, NULL, pins_passive},
    [PART_74HC08] = {PART_74HC08, "74hc08", NULL, 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE | COMPONENT_DIP, 22, NULL, pins_74hc00},
    [PART_74HC32] = {PART_74HC32, "74hc32", NULL, 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE | COMPONENT_DIP, 22, NULL, pins_74hc00},
    [PART_74HC86] = {PART_74HC86, "74hc86", NULL, 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE | COMPONENT_DIP, 22, NULL, pins_74hc00},
    [PART_74HC00] = {PART_74HC00, "74hc00", NULL, 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE | COMPONENT_DIP, 15, NULL, pins_74hc00},
    [PART_DIODE] = {PART_DIODE, "diode", NULL, 2, COMPONENT_ROTATES, 24, NULL, pins_led},
    [PART_LDR] = {PART_LDR, "ldr", NULL, 2, COMPONENT_ROTATES, 24, &force_resistance, pins_passive},
    [PART_FLEX] = {PART_FLEX, "flex", NULL, 2, COMPONENT_ROTATES, 24, &flex_resistance, pins_passive},
    [PART_FORCE] = {PART_FORCE, "force", NULL, 2, COMPONENT_ROTATES, 24, &force_resistance, pins_passive},
    [PART_TILT] = {PART_TILT, "tilt", NULL, 2, COMPONENT_ROTATES, 24, NULL, pins_passive},
    [PART_NPN] = {PART_NPN, "npn", NULL, 3, COMPONENT_ROTATES, 24, NULL, pins_bjt},
    [PART_PNP] = {PART_PNP, "pnp", NULL, 3, COMPONENT_ROTATES, 24, NULL, pins_bjt},
    [PART_BATTERY_9V] = {PART_BATTERY_9V, "battery-9v", NULL, 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_9v, pins_source},
    [PART_BATTERY_AA] = {PART_BATTERY_AA, "battery-aa", NULL, 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_aa, pins_source},
    [PART_BATTERY_COIN] = {PART_BATTERY_COIN, "battery-coin", NULL, 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_coin, pins_source},
    [PART_PHOTODIODE] = {PART_PHOTODIODE, "photodiode", NULL, 2, COMPONENT_ROTATES, 24, NULL, pins_led},
    [PART_AMBIENT_LIGHT] = {PART_AMBIENT_LIGHT, "ambient-light", NULL, 2, COMPONENT_ROTATES, 24, NULL, pins_photo_transistor},
    [PART_TEMPERATURE] = {PART_TEMPERATURE, "temperature", NULL, 3, COMPONENT_ROTATES, 24, NULL, pins_sensor},
    [PART_SOIL] = {PART_SOIL, "soil", NULL, 3, COMPONENT_ROTATES, 24, NULL, pins_sensor},
    [PART_LAMP] = {PART_LAMP, "lamp", NULL, 2, COMPONENT_ROTATES, 25, &lamp_resistance, pins_passive},
    [PART_RGB_LED] = {PART_RGB_LED, "rgb-led", NULL, 4, COMPONENT_ROTATES, 26, NULL, pins_rgb},
    [PART_SEVEN_SEGMENT] = {PART_SEVEN_SEGMENT, "seven-segment", NULL, 9, COMPONENT_ROTATES, 26, NULL, pins_seven},
    [PART_DIP_SWITCH] = {PART_DIP_SWITCH, "dip-switch", NULL, 8, COMPONENT_ROTATES, 26, NULL, pins_dip},
    [PART_DPST] = {PART_DPST, "dpst", NULL, 4, COMPONENT_ROTATES, 26, NULL, pins_dpst},
    [PART_RELAY] = {PART_RELAY, "relay", NULL, 5, COMPONENT_ROTATES, 26, NULL, pins_relay},
    [PART_DC_MOTOR] = {PART_DC_MOTOR, "dc-motor", NULL, 2, COMPONENT_ROTATES, 26, NULL, pins_motor},
    [PART_VIBRATION_MOTOR] = {PART_VIBRATION_MOTOR, "vibration-motor", NULL, 2, COMPONENT_ROTATES, 26, NULL, pins_motor},
    [PART_ENCODER_MOTOR] = {PART_ENCODER_MOTOR, "encoder-motor", NULL, 6, COMPONENT_ROTATES, 26, NULL, pins_encoder},
    [PART_SERVO] = {PART_SERVO, "servo", NULL, 3, COMPONENT_ROTATES, 26, NULL, pins_servo},
    [PART_PIEZO] = {PART_PIEZO, "piezo", NULL, 2, COMPONENT_ROTATES, 26, NULL, pins_motor},
    [PART_FUNCTION_GENERATOR] = {PART_FUNCTION_GENERATOR, "function-generator", NULL, 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 26, &generator_frequency, pins_generator},
    [PART_MULTIMETER] = {PART_MULTIMETER, "multimeter", NULL, 3, 0, 26, NULL, pins_meter},
    [PART_OSCILLOSCOPE] = {PART_OSCILLOSCOPE, "oscilloscope", NULL, 2, 0, 26, NULL, pins_scope},
    [PART_PIR] = {PART_PIR, "pir", NULL, 3, COMPONENT_ROTATES, 26, NULL, pins_sensor},
    [PART_GAS] = {PART_GAS, "gas", NULL, 3, COMPONENT_ROTATES, 26, NULL, pins_sensor},
    [PART_IR_OBSTACLE] = {PART_IR_OBSTACLE, "ir-obstacle", NULL, 3, COMPONENT_ROTATES, 26, NULL, pins_sensor},
    [PART_DISTANCE] = {PART_DISTANCE, "distance", NULL, 3, COMPONENT_ROTATES, 26, NULL, pins_sensor},
    [PART_ARDUINO_UNO] = {PART_ARDUINO_UNO, "arduino-uno", NULL, 23, COMPONENT_ROTATES, 27, NULL, pins_uno},
    [PART_ESP32] = {PART_ESP32, "esp32", NULL, 22, COMPONENT_ROTATES, 27, NULL, pins_esp32},
    [PART_ATTINY85] = {PART_ATTINY85, "attiny85", NULL, 8, COMPONENT_ROTATES, 27, NULL, pins_tiny},
    [PART_LCD_I2C] = {PART_LCD_I2C, "lcd-16x2-i2c", NULL, 4, COMPONENT_ROTATES, 27, NULL, pins_lcd},
    [PART_JUNCTION] = {PART_JUNCTION, "junction", NULL, 1, 0, 31, NULL, pins_junction},
    [PART_74HC161] = {PART_74HC161, "74hc161", NULL, 16, COMPONENT_ROTATES | COMPONENT_DIP, 35, NULL, pins_74hc161},
    [PART_74HC173] = {PART_74HC173, "74hc173", NULL, 16, COMPONENT_ROTATES | COMPONENT_DIP, 35, NULL, pins_74hc173},
    [PART_74HC245] = {PART_74HC245, "74hc245", NULL, 20, COMPONENT_ROTATES | COMPONENT_DIP, 35, NULL, pins_74hc245},
    [PART_74HC283] = {PART_74HC283, "74hc283", NULL, 16, COMPONENT_ROTATES | COMPONENT_DIP, 35, NULL, pins_74hc283},
    [PART_AT28C16] = {PART_AT28C16, "at28c16", NULL, 24, COMPONENT_ROTATES | COMPONENT_DIP, 35, NULL, pins_at28c16},
    [PART_NOTE] = {PART_NOTE, "note", NULL, 0, 0, 37, NULL, NULL},
    [PART_BREADBOARD_WIDE] = {PART_BREADBOARD_WIDE, "breadboard-wide", NULL, BREADBOARD_WIDE_PINS, COMPONENT_CONNECTIVITY, 36, NULL, NULL},
};

/* Display names; component_info() returns them in the current language. */
static const TextId display_texts[] = {
    [PART_SOURCE] = TXT_CAT_SOURCE,
    [PART_SWITCH] = TXT_CAT_SWITCH,
    [PART_LED] = TXT_CAT_LED,
    [PART_BREADBOARD] = TXT_CAT_BREADBOARD,
    [PART_RESISTOR] = TXT_CAT_RESISTOR,
    [PART_CAPACITOR] = TXT_CAT_CAPACITOR,
    [PART_INDUCTOR] = TXT_CAT_INDUCTOR,
    [PART_AND] = TXT_CAT_AND,
    [PART_NAND] = TXT_CAT_NAND,
    [PART_OR] = TXT_CAT_OR,
    [PART_XOR] = TXT_CAT_XOR,
    [PART_NOT] = TXT_CAT_NOT,
    [PART_CLOCK] = TXT_CAT_CLOCK,
    [PART_DFF] = TXT_CAT_DFF,
    [PART_NOR] = TXT_CAT_NOR,
    [PART_XNOR] = TXT_CAT_XNOR,
    [PART_POTENTIOMETER] = TXT_CAT_POTENTIOMETER,
    [PART_BUTTON] = TXT_CAT_BUTTON,
    [PART_74HC08] = TXT_CAT_74HC08,
    [PART_74HC32] = TXT_CAT_74HC32,
    [PART_74HC86] = TXT_CAT_74HC86,
    [PART_74HC00] = TXT_CAT_74HC00,
    [PART_74HC161] = TXT_CAT_74HC161,
    [PART_74HC173] = TXT_CAT_74HC173,
    [PART_74HC245] = TXT_CAT_74HC245,
    [PART_74HC283] = TXT_CAT_74HC283,
    [PART_AT28C16] = TXT_CAT_AT28C16,
    [PART_BREADBOARD_WIDE] = TXT_CAT_BREADBOARD_WIDE,
    [PART_NOTE] = TXT_CAT_NOTE,
    [PART_DIODE] = TXT_CAT_DIODE,
    [PART_LDR] = TXT_CAT_LDR,
    [PART_FLEX] = TXT_CAT_FLEX,
    [PART_FORCE] = TXT_CAT_FORCE,
    [PART_TILT] = TXT_CAT_TILT,
    [PART_NPN] = TXT_CAT_NPN,
    [PART_PNP] = TXT_CAT_PNP,
    [PART_BATTERY_9V] = TXT_CAT_BATTERY_9V,
    [PART_BATTERY_AA] = TXT_CAT_BATTERY_AA,
    [PART_BATTERY_COIN] = TXT_CAT_BATTERY_COIN,
    [PART_PHOTODIODE] = TXT_CAT_PHOTODIODE,
    [PART_AMBIENT_LIGHT] = TXT_CAT_AMBIENT_LIGHT,
    [PART_TEMPERATURE] = TXT_CAT_TEMPERATURE,
    [PART_SOIL] = TXT_CAT_SOIL,
    [PART_LAMP] = TXT_CAT_LAMP,
    [PART_RGB_LED] = TXT_CAT_RGB_LED,
    [PART_SEVEN_SEGMENT] = TXT_CAT_SEVEN_SEGMENT,
    [PART_DIP_SWITCH] = TXT_CAT_DIP_SWITCH,
    [PART_DPST] = TXT_CAT_DPST,
    [PART_RELAY] = TXT_CAT_RELAY,
    [PART_DC_MOTOR] = TXT_CAT_DC_MOTOR,
    [PART_VIBRATION_MOTOR] = TXT_CAT_VIBRATION_MOTOR,
    [PART_ENCODER_MOTOR] = TXT_CAT_ENCODER_MOTOR,
    [PART_SERVO] = TXT_CAT_SERVO,
    [PART_PIEZO] = TXT_CAT_PIEZO,
    [PART_FUNCTION_GENERATOR] = TXT_CAT_FUNCTION_GENERATOR,
    [PART_MULTIMETER] = TXT_CAT_MULTIMETER,
    [PART_OSCILLOSCOPE] = TXT_CAT_OSCILLOSCOPE,
    [PART_PIR] = TXT_CAT_PIR,
    [PART_GAS] = TXT_CAT_GAS,
    [PART_IR_OBSTACLE] = TXT_CAT_IR_OBSTACLE,
    [PART_DISTANCE] = TXT_CAT_DISTANCE,
    [PART_ARDUINO_UNO] = TXT_CAT_ARDUINO_UNO,
    [PART_ESP32] = TXT_CAT_ESP32,
    [PART_ATTINY85] = TXT_CAT_ATTINY85,
    [PART_LCD_I2C] = TXT_CAT_LCD_I2C,
    [PART_JUNCTION] = TXT_CAT_JUNCTION,
};

/* Pins whose names are words; symbols such as GND or 1A are the same in every language. */
typedef struct
{
    PartType type;
    int pin;
    TextId name;
} PinText;

static const PinText pin_texts[] = {
    {PART_SERVO, 2, TXT_CAT_PIN_SIGNAL},
    {PART_FUNCTION_GENERATOR, 0, TXT_CAT_PIN_OUTPUT},
    {PART_RELAY, 3, TXT_CAT_PIN_NORMALLY_OPEN},
    {PART_RELAY, 4, TXT_CAT_PIN_NORMALLY_CLOSED},
};

/* Environment controls: the stimulus a sensor reads and the limits of its model. */
typedef struct
{
    PartType type;
    TextId label;
    const char *unit;
    double minimum, maximum, initial;
    TextId note;
} EnvironmentText;

static const EnvironmentText environment_texts[] = {
    {PART_LDR, TXT_CAT_ENV_LIGHT, "lux", 0, 1000, 100, TXT_CAT_ENV_LIGHT_NOTE},
    {PART_FLEX, TXT_CAT_ENV_BEND, "°", 0, 90, 0, TXT_CAT_ENV_BEND_NOTE},
    {PART_FORCE, TXT_CAT_ENV_FORCE, "N", 0, 20, 0, TXT_CAT_ENV_FORCE_NOTE},
    {PART_TILT, TXT_CAT_ENV_TILT, "°", 0, 180, 0, TXT_CAT_ENV_TILT_NOTE},
    {PART_PHOTODIODE, TXT_CAT_ENV_PHOTO, "%", 0, 100, 0, TXT_CAT_ENV_PHOTO_NOTE},
    {PART_AMBIENT_LIGHT, TXT_CAT_ENV_PHOTO, "%", 0, 100, 0, TXT_CAT_ENV_PHOTO_NOTE},
    {PART_TEMPERATURE, TXT_CAT_ENV_TEMPERATURE, "°C", -40, 125, 25, TXT_CAT_ENV_TEMPERATURE_NOTE},
    {PART_SOIL, TXT_CAT_ENV_SOIL, "%", 0, 100, 25, TXT_CAT_ENV_SOIL_NOTE},
    {PART_PIR, TXT_CAT_ENV_MOTION, "%", 0, 100, 0, TXT_CAT_ENV_MOTION_NOTE},
    {PART_GAS, TXT_CAT_ENV_GAS, "ppm", 0, 1000, 100, TXT_CAT_ENV_GAS_NOTE},
    {PART_IR_OBSTACLE, TXT_CAT_ENV_OBSTACLE, "cm", 0, 30, 20, TXT_CAT_ENV_OBSTACLE_NOTE},
    {PART_DISTANCE, TXT_CAT_ENV_DISTANCE, "cm", 10, 80, 30, TXT_CAT_ENV_DISTANCE_NOTE},
};

enum
{
    COMPONENT_TABLE_SIZE = sizeof(components) / sizeof(components[0]),
    PIN_TEXT_COUNT = sizeof(pin_texts) / sizeof(pin_texts[0]),
    ENVIRONMENT_COUNT = sizeof(environment_texts) / sizeof(environment_texts[0])
};

_Static_assert(sizeof(display_texts) / sizeof(display_texts[0]) == COMPONENT_TABLE_SIZE,
               "every part needs a display text");

/* The tables above with their texts in one language; callers keep plain const char * fields. */
typedef struct
{
    ComponentInfo components[COMPONENT_TABLE_SIZE];
    ComponentPinInfo pins[PIN_TEXT_COUNT];
    ComponentPinInfo board_contact;
    ComponentEnvironment environments[ENVIRONMENT_COUNT];
} LocalizedCatalog;

static LocalizedCatalog localized[UI_LANG_COUNT];
static int localized_ready;

static void localize_components(UiLanguage language, LocalizedCatalog *catalog)
{
    for (size_t i = 0; i < COMPONENT_TABLE_SIZE; ++i)
    {
        catalog->components[i] = components[i];
        if (components[i].id)
            catalog->components[i].display_name = tr_in(language, display_texts[i]);
    }
}

static void localize_pins(UiLanguage language, LocalizedCatalog *catalog)
{
    for (size_t i = 0; i < PIN_TEXT_COUNT; ++i)
    {
        const PinText *text = &pin_texts[i];
        catalog->pins[i] = components[text->type].pins[text->pin];
        catalog->pins[i].name = tr_in(language, text->name);
    }
    catalog->board_contact.name = tr_in(language, TXT_CAT_PIN_CONTACT);
    catalog->board_contact.role = PIN_CONTACT;
}

static void localize_environments(UiLanguage language, LocalizedCatalog *catalog)
{
    for (size_t i = 0; i < ENVIRONMENT_COUNT; ++i)
    {
        const EnvironmentText *text = &environment_texts[i];
        ComponentEnvironment *environment = &catalog->environments[i];
        environment->label = tr_in(language, text->label);
        environment->unit = text->unit;
        environment->minimum = text->minimum;
        environment->maximum = text->maximum;
        environment->initial = text->initial;
        environment->note = tr_in(language, text->note);
    }
}

/* Built once, on first use from the main thread, for every language. The texts are static, so a pointer
 * taken before a language switch stays valid (it keeps the old language). */
static const LocalizedCatalog *localized_catalog(void)
{
    if (!localized_ready)
    {
        for (int language = 0; language < UI_LANG_COUNT; ++language)
        {
            localize_components((UiLanguage)language, &localized[language]);
            localize_pins((UiLanguage)language, &localized[language]);
            localize_environments((UiLanguage)language, &localized[language]);
        }
        localized_ready = 1;
    }
    return &localized[ui_text_language()];
}

/* Historical integer codes are fixed by file versions 1-16, independent of enum order. */
static const PartType legacy_types[] = {PART_SOURCE,    PART_SWITCH,   PART_LED,   PART_BREADBOARD, PART_RESISTOR,
                                        PART_CAPACITOR, PART_INDUCTOR, PART_AND,   PART_NAND,       PART_OR,
                                        PART_XOR,       PART_NOT,      PART_CLOCK, PART_DFF,        PART_74HC00, PART_NOR, PART_XNOR, PART_POTENTIOMETER, PART_BUTTON, PART_74HC08, PART_74HC32, PART_74HC86, PART_DIODE, PART_LDR, PART_FLEX, PART_FORCE, PART_TILT, PART_NPN, PART_PNP, PART_BATTERY_9V, PART_BATTERY_AA, PART_BATTERY_COIN, PART_PHOTODIODE, PART_AMBIENT_LIGHT, PART_TEMPERATURE, PART_SOIL, PART_LAMP, PART_RGB_LED, PART_SEVEN_SEGMENT, PART_DIP_SWITCH, PART_DPST, PART_RELAY, PART_DC_MOTOR, PART_VIBRATION_MOTOR, PART_ENCODER_MOTOR, PART_SERVO, PART_PIEZO, PART_FUNCTION_GENERATOR, PART_MULTIMETER, PART_OSCILLOSCOPE, PART_PIR, PART_GAS, PART_IR_OBSTACLE, PART_DISTANCE, PART_ARDUINO_UNO, PART_ESP32, PART_ATTINY85, PART_LCD_I2C, PART_JUNCTION, PART_74HC161, PART_74HC173, PART_74HC245, PART_74HC283, PART_AT28C16, PART_BREADBOARD_WIDE, PART_NOTE};

size_t component_registry_count(void)
{
    return sizeof(components) / sizeof(components[0]);
}

const ComponentInfo *component_info(PartType type)
{
    if ((unsigned)type >= component_registry_count() || !components[type].id)
        return NULL;
    return &localized_catalog()->components[type];
}

const ComponentInfo *component_from_legacy(int code, int version)
{
    if ((unsigned)code >= sizeof(legacy_types) / sizeof(legacy_types[0]))
        return NULL;
    const ComponentInfo *info = component_info(legacy_types[code]);
    return info && version >= info->first_file_version ? info : NULL;
}

const ComponentInfo *component_by_id(const char *id)
{
    if (!id)
        return NULL;
    for (size_t i = 0; i < component_registry_count(); ++i)
        if (components[i].id && !strcmp(components[i].id, id))
            return &localized_catalog()->components[i];
    return NULL;
}

int component_value_valid(PartType type, double value)
{
    const ComponentInfo *info = component_info(type);
    return info && info->primary && isfinite(value) && value >= info->primary->minimum &&
           value <= info->primary->maximum;
}

int component_has_flag(PartType type, unsigned flag)
{
    const ComponentInfo *info = component_info(type);
    return info && (info->flags & flag) == flag;
}

int component_legacy_supported(PartType type, int version)
{
    const ComponentInfo *info = component_info(type);
    return info && version >= info->first_file_version;
}

const ComponentPinInfo *component_pin_info(PartType type, int pin)
{
    const ComponentInfo *info = component_info(type);
    if (!info || pin < 0 || pin >= info->pin_count)
        return NULL;
    if (!info->pins)
        return &localized_catalog()->board_contact;
    for (size_t i = 0; i < PIN_TEXT_COUNT; ++i)
        if (pin_texts[i].type == type && pin_texts[i].pin == pin)
            return &localized_catalog()->pins[i];
    return &info->pins[pin];
}

const char *component_pin_role_label(ComponentPinRole role)
{
    static const TextId labels[] = {
        TXT_CAT_ROLE_PASSIVE,  TXT_CAT_ROLE_INPUT,   TXT_CAT_ROLE_CLOCK_INPUT, TXT_CAT_ROLE_OUTPUT,
        TXT_CAT_ROLE_SUPPLY_POSITIVE, TXT_CAT_ROLE_SUPPLY_RETURN, TXT_CAT_ROLE_GROUND, TXT_CAT_ROLE_ANODE,
        TXT_CAT_ROLE_CATHODE,  TXT_CAT_ROLE_CONTACT, TXT_CAT_ROLE_COLLECTOR,   TXT_CAT_ROLE_BASE,
        TXT_CAT_ROLE_EMITTER,  TXT_CAT_ROLE_BIDIRECTIONAL};
    if ((unsigned)role >= sizeof(labels) / sizeof(labels[0]))
        return tr(TXT_CAT_ROLE_UNKNOWN);
    return tr(labels[role]);
}

typedef struct
{
    PartType type;
    const char *symbol;
} PartSymbol;

const char *component_symbol(PartType type)
{
    static const PartSymbol symbols[] = {
        {PART_AND, "AND"},       {PART_NAND, "NAND"},     {PART_OR, "OR"},         {PART_XOR, "XOR"},
        {PART_NOT, "NOT"},       {PART_NOR, "NOR"},       {PART_XNOR, "XNOR"},     {PART_DFF, "DFF"},
        {PART_74HC08, "74HC08"}, {PART_74HC32, "74HC32"}, {PART_74HC86, "74HC86"}, {PART_74HC00, "74HC00"},
        {PART_74HC161, "74HC161"}, {PART_74HC173, "74HC173"}, {PART_74HC245, "74HC245"}, {PART_74HC283, "74HC283"},
        {PART_AT28C16, "AT28C16"}};
    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
        if (symbols[i].type == type)
            return symbols[i].symbol;
    return NULL;
}

const char *component_pin_marker(ComponentPinRole role)
{
    static const char *marks[] = {"IO", "IN", "CLK", "OUT", "V+", "V-", "GND", "A", "K", "CONT", "C", "B", "E", "I/O"};
    if ((unsigned)role >= sizeof(marks) / sizeof(marks[0]))
        return "?";
    return marks[role];
}

PartType component_electrical_type(PartType type)
{
    if (bench_is_board(type))
        return PART_BREADBOARD;
    switch (type)
    {
    case PART_LDR:
    case PART_FLEX:
    case PART_FORCE:
    case PART_LAMP:
        return PART_RESISTOR;
    case PART_TILT:
        return PART_SWITCH;
    case PART_BATTERY_9V:
    case PART_BATTERY_AA:
    case PART_BATTERY_COIN:
        return PART_SOURCE;
    default:
        return type;
    }
}

double component_effective_value(const Part *part)
{
    double stimulus = component_environment_value(part);
    if (part->type == PART_LDR)
        return fmin(1e8, fmax(10, part->value * pow(10 / fmax(stimulus, .001), .7)));
    if (part->type == PART_FLEX)
        return part->value * (1 + 3 * part->control);
    if (part->type == PART_FORCE)
        return stimulus < .2 ? 1e8 : part->value / stimulus;
    if (part->type == PART_SOIL)
        return 100000 * pow(100, -part->control);
    return part->value;
}

int component_effective_on(const Part *part)
{
    return part->type == PART_TILT ? component_environment_value(part) < 45 : part->on;
}

double component_default_control(PartType type)
{
    const ComponentEnvironment *environment = component_environment(type);
    return environment ? (environment->initial - environment->minimum) / (environment->maximum - environment->minimum) : .5;
}

int component_current_vector(PartType type, int index, char *vector, size_t capacity)
{
    if (type == PART_LED || type == PART_DIODE || type == PART_PHOTODIODE)
        snprintf(vector, capacity, "@d%d[id]", index);
    else if (component_electrical_type(type) == PART_RESISTOR)
        snprintf(vector, capacity, "@r%d[i]", index);
    else if (type == PART_SOIL)
        snprintf(vector, capacity, "@rsoil%d[i]", index);
    else if (type == PART_TEMPERATURE)
        snprintf(vector, capacity, "@vtmp_sense%d[i]", index);
    else if (type == PART_NPN || type == PART_PNP || type == PART_AMBIENT_LIGHT)
        snprintf(vector, capacity, "@q%d[ic]", index);
    else
        return 0;
    return 1;
}


const ComponentEnvironment *component_environment(PartType type)
{
    for (size_t i = 0; i < ENVIRONMENT_COUNT; ++i)
        if (environment_texts[i].type == type)
            return &localized_catalog()->environments[i];
    return NULL;
}

double component_environment_value(const Part *part)
{
    const ComponentEnvironment *environment = component_environment(part->type);
    return environment ? environment->minimum + part->control * (environment->maximum - environment->minimum) : 0;
}

double component_source_resistance(PartType type)
{
    if (type == PART_BATTERY_9V)
        return 1;
    if (type == PART_BATTERY_AA)
        return .2;
    if (type == PART_BATTERY_COIN)
        return 15;
    /* Bench supply: same as a closed live contact (NETLIST_LIVE_CLOSED_OHMS), keeping the matrix conditioned. */
    return 1e-3;
}

int component_radial_pins(PartType type)
{
    return component_environment(type) != NULL || type == PART_NPN || type == PART_PNP;
}


void component_body_extent(PartType type, float *half_width, float *half_height)
{
    const ComponentDevice *device = component_device(type);
    if (device)
    {
        *half_width = device->half_width;
        *half_height = device->half_height;
        return;
    }
    *half_width = 44;
    *half_height = 30;
    if (type == PART_NOTE)
    {
        *half_width = 120;
        *half_height = 55;
    }
    else if (type == PART_JUNCTION)
    {
        *half_width = 4;
        *half_height = 4;
    }
    else if (type == PART_LED)
    {
        *half_width = 17;
        *half_height = 28;
    }
    else if (type == PART_SOURCE)
    {
        *half_width = 41;
        *half_height = 27;
    }
    else if (type == PART_SWITCH)
    {
        *half_width = 30;
        *half_height = 21;
    }
    else if (type == PART_CLOCK)
    {
        *half_width = 34;
        *half_height = 26;
    }
    else if (type == PART_BUTTON)
    {
        *half_width = 25;
        *half_height = 22;
    }
    else if (type == PART_RESISTOR || type == PART_DIODE)
    {
        *half_width = 21;
        *half_height = 8;
    }
    else if (type == PART_CAPACITOR)
    {
        *half_width = 25;
        *half_height = 22;
    }
    else if (type == PART_INDUCTOR)
    {
        *half_width = 34;
        *half_height = 14;
    }
    else if (type == PART_POTENTIOMETER)
    {
        *half_width = 28;
        *half_height = 30;
    }
    else if (type == PART_BATTERY_COIN)
    {
        *half_width = 27;
        *half_height = 27;
    }
    else if (type == PART_NPN || type == PART_PNP || type == PART_TEMPERATURE)
    {
        *half_width = 19;
        *half_height = 22;
    }
    else if (component_has_flag(type, COMPONENT_GATE_SYMBOL))
    {
        *half_width = 31;
        *half_height = 27;
    }
    else if (bench_is_board(type))
    {
        *half_width = type == PART_BREADBOARD_WIDE ? 437 : 227;
        *half_height = 112;
    }
    else if (component_has_flag(type, COMPONENT_DIP))
    {
        *half_width = (float)((component_info(type)->pin_count / 2 - 1) * BENCH_DIP_PITCH / 2 + 8);
        *half_height = 20;
    }
    else if (type == PART_BATTERY_9V || type == PART_BATTERY_AA)
    {
        *half_width = type == PART_BATTERY_AA ? 15 : 27;
        *half_height = 48;
    }
    else if (component_radial_pins(type))
    {
        *half_width = type == PART_FLEX ? 8 : 22;
        *half_height = type == PART_FLEX || type == PART_FORCE || type == PART_SOIL ? 42 : 30;
    }
}


const ComponentTemperatureModel *component_temperature_model(void)
{
    /* ADI nominal transfer; output/limiting resistances are didactic choices. */
    static const ComponentTemperatureModel model = {.offset = .5, .volts_per_degree = .01,
        .supply_minimum = 2.7, .supply_maximum = 5.5, .quiescent_current = 50e-6,
        .output_resistance = 100, .current_limit = 250e-6, .limiting_resistance = 1e9, .off_resistance = 1e12};
    return &model;
}

double component_photocurrent(const Part *part)
{
    const double photodiode_maximum_amps = .001;
    const double phototransistor_base_maximum_amps = .00001;
    return part->control * (part->type == PART_PHOTODIODE ? photodiode_maximum_amps : phototransistor_base_maximum_amps);
}
