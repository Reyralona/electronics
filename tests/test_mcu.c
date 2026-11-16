#include "mcu_runtime.h"
#include "spice_live.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *executable;
static SimAutoOptions options;
static SimAutoResult result;

enum
{
    UNO_D13 = 13,
    UNO_A0 = 14,
    UNO_A4 = 18,
    UNO_A5 = 19,
    UNO_5V = 20,
    UNO_GND = 22
};

static void wire(Workbench *bench, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}

static void live(SpiceLive *session, const Workbench *bench)
{
    int success = spice_live_step(session, bench, executable, &options, .0005, &result);
    if (!success)
        fprintf(stderr, "MCU live: %s\n", result.message);
    assert(success);
}

static void blink_drives_led(void)
{
    Workbench bench = {0};
    int uno = bench_add(&bench, PART_ARDUINO_UNO, 400, 300), resistor = bench_add(&bench, PART_RESISTOR, 700, 120);
    int led = bench_add(&bench, PART_LED, 860, 120);
    assert(uno == 0 && bench_set_value(&bench, resistor, 220));
    wire(&bench, uno, UNO_D13, resistor, 0);
    wire(&bench, resistor, 1, led, 0);
    wire(&bench, led, 1, uno, UNO_GND);
    McuRuntime *runtime = mcu_runtime_create();
    SpiceLive *session = spice_live_create();
    mcu_runtime_step(runtime, &bench, NULL, 0);
    assert(bench.parts[uno].io_mode[13] == MCU_OUTPUT && bench.parts[uno].io_level[13] == 1);
    live(session, &bench);
    assert(result.powered && result.currents[led] > .005);
    mcu_runtime_step(runtime, &bench, (const double(*)[COMPONENT_MAX_ELEMENTS])result.elements, 600);
    assert(bench.parts[uno].io_level[13] == 0);
    live(session, &bench);
    assert(result.currents[led] < 1e-5);
    spice_live_destroy(session);
    mcu_runtime_destroy(runtime);
    bench_snapshot_clear(&bench);
}

static void analog_and_lcd(void)
{
    Workbench bench = {0};
    int uno = bench_add(&bench, PART_ARDUINO_UNO, 400, 300), pot = bench_add(&bench, PART_POTENTIOMETER, 700, 500);
    int lcd = bench_add(&bench, PART_LCD_I2C, 900, 300);
    wire(&bench, uno, UNO_5V, pot, 0);
    wire(&bench, pot, 2, uno, UNO_GND);
    wire(&bench, pot, 1, uno, UNO_A0);
    wire(&bench, lcd, 0, uno, UNO_GND);
    wire(&bench, lcd, 1, uno, UNO_5V);
    wire(&bench, lcd, 2, uno, UNO_A4);
    wire(&bench, lcd, 3, uno, UNO_A5);
    assert(bench_set_code(&bench, uno,
                          "LiquidCrystal_I2C lcd(0x27, 16, 2);\n"
                          "void setup() { Serial.begin(9600); lcd.init(); lcd.backlight(); lcd.print(\"Ola\"); }\n"
                          "void loop() { int v = analogRead(A0); Serial.println(v); lcd.setCursor(0, 1); lcd.print(v);"
                          " delay(100); }\n"));
    McuRuntime *runtime = mcu_runtime_create();
    SpiceLive *session = spice_live_create();
    mcu_runtime_step(runtime, &bench, NULL, 0);
    assert(!strncmp(runtime->lcd[lcd].text[0], "Ola", 3) && runtime->lcd[lcd].backlight);
    live(session, &bench);
    assert(result.elements[lcd][0] > 4.9);
    assert(fabs(result.elements[uno][14] - 2.5) < .05);
    runtime->mcu[uno].serial[0] = 0;
    mcu_runtime_step(runtime, &bench, (const double(*)[COMPONENT_MAX_ELEMENTS])result.elements, 150);
    int reading = 0;
    assert(sscanf(runtime->mcu[uno].serial, "%d", &reading) == 1 && reading > 490 && reading < 530);
    assert(!strncmp(runtime->lcd[lcd].text[1], "5", 1));
    spice_live_destroy(session);
    mcu_runtime_destroy(runtime);
    bench_snapshot_clear(&bench);
}

static void compile_error_releases_pins(void)
{
    Workbench bench = {0};
    int uno = bench_add(&bench, PART_ARDUINO_UNO, 400, 300);
    assert(bench_set_code(&bench, uno, "void setup() { pinMode(13 OUTPUT); }\nvoid loop() {}"));
    McuRuntime *runtime = mcu_runtime_create();
    mcu_runtime_step(runtime, &bench, NULL, 0);
    assert(runtime->mcu[uno].error[0] && runtime->mcu[uno].error_line == 1);
    assert(bench.parts[uno].io_mode[13] == MCU_INPUT);
    mcu_runtime_destroy(runtime);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    compile_error_releases_pins();
    assert(argc == 2);
    executable = argv[1];
    options = (SimAutoOptions){0, {0, 0}, 1e-5, .005, burn_limits_default()};
    blink_drives_led();
    analog_and_lcd();
    puts("MCU boards: blink drives an LED, analogRead follows a pot, LCD over I2C pins and compile errors passed.");
    return 0;
}
