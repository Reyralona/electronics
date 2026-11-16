#include "mcu_board.h"

static const McuConstant uno_constants[] = {{"LED_BUILTIN", 13}, {"A0", 14}, {"A1", 15}, {"A2", 16},
                                            {"A3", 17},          {"A4", 18}, {"A5", 19}, {"SDA", 18},
                                            {"SCL", 19}};
static const McuConstant esp32_constants[] = {{"LED_BUILTIN", 2}, {"SDA", 21}, {"SCL", 22}};
static const McuConstant tiny_constants[] = {{"LED_BUILTIN", 1}, {"A1", 2}, {"A2", 4}, {"A3", 3}};

static const char blink_uno[] = "// Pisca o LED da placa (pino 13)\n"
                                "void setup() {\n"
                                "  pinMode(LED_BUILTIN, OUTPUT);\n"
                                "  Serial.begin(9600);\n"
                                "}\n\n"
                                "void loop() {\n"
                                "  digitalWrite(LED_BUILTIN, HIGH);\n"
                                "  delay(500);\n"
                                "  digitalWrite(LED_BUILTIN, LOW);\n"
                                "  delay(500);\n"
                                "}\n";

static const McuBoard boards[] = {
    {PART_ARDUINO_UNO, "Arduino Uno", 20,
     {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
     {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}, 20, 22, 5.0, 21, 3.3, 1023, 18, 19, 13,
     uno_constants, sizeof(uno_constants) / sizeof(uno_constants[0]), blink_uno},
    {PART_ESP32, "ESP32 DevKit", 20,
     {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
     {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34}, 20, 21, 3.3, -1, 0, 4095, 21, 22,
     2, esp32_constants, sizeof(esp32_constants) / sizeof(esp32_constants[0]),
     "// ESP32: LED no GPIO 2\nvoid setup() {\n  pinMode(2, OUTPUT);\n}\n\nvoid loop() {\n  digitalWrite(2, HIGH);\n"
     "  delay(300);\n  digitalWrite(2, LOW);\n  delay(300);\n}\n"},
    {PART_ATTINY85, "ATtiny85", 6, {4, 5, 6, 1, 2, 0}, {0, 1, 2, 3, 4, 5}, 7, 3, 0, -1, 0, 1023, 0, 2, 1,
     tiny_constants, sizeof(tiny_constants) / sizeof(tiny_constants[0]),
     "// ATtiny85: alimente VCC (pino 8) e GND (pino 4)\nvoid setup() {\n  pinMode(1, OUTPUT);\n}\n\nvoid loop() {\n"
     "  digitalWrite(1, HIGH);\n  delay(250);\n  digitalWrite(1, LOW);\n  delay(250);\n}\n"},
};

const McuBoard *mcu_board(PartType type)
{
    for (size_t i = 0; i < sizeof(boards) / sizeof(boards[0]); ++i)
        if (boards[i].type == type)
            return &boards[i];
    return NULL;
}

int mcu_board_io(const McuBoard *board, int arduino_pin)
{
    for (int k = 0; board && k < board->io_count; ++k)
        if (board->arduino_pin[k] == arduino_pin)
            return k;
    return -1;
}
