#include "mcu_lang.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    int modes[32], levels[32], reads[32], analog[32], duty[32];
    char serial[512], lcd[2][17];
    int column, row, backlight;
} FakeBoard;

static void pin_mode(void *c, int pin, int mode) { ((FakeBoard *)c)->modes[pin] = mode; }
static void digital_write(void *c, int pin, int level) { ((FakeBoard *)c)->levels[pin] = level; }
static int digital_read(void *c, int pin) { return ((FakeBoard *)c)->reads[pin]; }
static int analog_read(void *c, int pin) { return ((FakeBoard *)c)->analog[pin]; }
static void analog_write(void *c, int pin, int duty) { ((FakeBoard *)c)->duty[pin] = duty; }
static void serial_write(void *c, const char *text) { strncat(((FakeBoard *)c)->serial, text, 400); }

static void lcd(void *c, int object, McuLcdOperation operation, int a, int b, const char *text)
{
    FakeBoard *board = c;
    (void)object;
    if (operation == MCU_LCD_CLEAR || operation == MCU_LCD_INIT)
        memset(board->lcd, ' ', sizeof(board->lcd)), board->lcd[0][16] = board->lcd[1][16] = 0, board->column = board->row = 0;
    else if (operation == MCU_LCD_SET_CURSOR)
        board->column = a, board->row = b;
    else if (operation == MCU_LCD_BACKLIGHT)
        board->backlight = 1;
    else if (operation == MCU_LCD_PRINT)
        for (const char *s = text; *s && board->column < 16; ++s)
            board->lcd[board->row][board->column++] = *s;
}

static McuVm *start(const char *source, FakeBoard *board, McuHost *host, McuProgram **program)
{
    static const McuConstant constants[] = {{"LED_BUILTIN", 13}, {"A0", 14}};
    char error[200];
    int line = 0;
    *program = mcu_compile(source, constants, 2, error, sizeof(error), &line);
    if (!*program)
        fprintf(stderr, "%s\n", error);
    assert(*program);
    memset(board, 0, sizeof(*board));
    *host = (McuHost){board, pin_mode, digital_write, digital_read, analog_read, analog_write, serial_write, lcd};
    McuVm *vm = mcu_vm_create(*program);
    assert(vm);
    return vm;
}

static void run(McuVm *vm, McuHost *host, double ms)
{
    char error[200];
    int ok = mcu_vm_run(vm, host, ms, error, sizeof(error));
    if (!ok)
        fprintf(stderr, "%s\n", error);
    assert(ok);
}

static void blink(void)
{
    FakeBoard board;
    McuHost host;
    McuProgram *program;
    McuVm *vm = start("void setup() { pinMode(LED_BUILTIN, OUTPUT); }\n"
                      "void loop() { digitalWrite(LED_BUILTIN, HIGH); delay(500); digitalWrite(LED_BUILTIN, LOW); delay(500); }",
                      &board, &host, &program);
    run(vm, &host, 0);
    assert(board.modes[13] == MCU_OUTPUT && board.levels[13] == 1);
    run(vm, &host, 499);
    assert(board.levels[13] == 1);
    run(vm, &host, 500);
    assert(board.levels[13] == 0);
    run(vm, &host, 1000);
    assert(board.levels[13] == 1);
    mcu_vm_free(vm);
    mcu_program_free(program);
}

static void language(void)
{
    FakeBoard board;
    McuHost host;
    McuProgram *program;
    McuVm *vm = start("#include <Wire.h>\n#define LIMIT 5\n"
                      "LiquidCrystal_I2C lcd(0x27, 16, 2);\n"
                      "int table[4] = {3, 1, 4, 1};\nfloat half = 0.5;\nint counter;\n"
                      "int square(int x) { return x * x; }\n"
                      "void setup() {\n  Serial.begin(9600); lcd.init(); lcd.backlight();\n"
                      "  int sum = 0;\n  for (int i = 0; i < 4; i++) { if (table[i] == 1) continue; sum += table[i]; }\n"
                      "  Serial.println(sum);\n  Serial.println(7 / 2);\n  Serial.println(7 / 2.0);\n"
                      "  Serial.println(square(LIMIT) > 20 && !false ? 1 : 0);\n"
                      "  int n = 0; while (true) { n++; if (n >= 3) break; }\n  Serial.println(n);\n"
                      "  lcd.setCursor(0, 1); lcd.print(\"Oi \"); lcd.print(42);\n"
                      "  analogWrite(9, map(analogRead(A0), 0, 1023, 0, 255));\n}\n"
                      "void loop() { counter++; delay(10); }",
                      &board, &host, &program);
    board.analog[14] = 1023;
    run(vm, &host, 0);
    assert(!strcmp(board.serial, "7\n3\n3.50\n1\n3\n"));
    assert(!strncmp(board.lcd[1], "Oi 42", 5) && board.backlight);
    assert(board.duty[9] == 255);
    run(vm, &host, 100);
    mcu_vm_free(vm);
    mcu_program_free(program);
}

/* error must be "Line N: reason" in the interface language. */
static void assert_line_error(const char *error, int line, TextId reason, const char *argument)
{
    char text[160];
    char expected[200];
    snprintf(text, sizeof(text), tr(reason), argument);
    snprintf(expected, sizeof(expected), tr(TXT_MCU_ERROR_LINE), line, text);
    assert(!strcmp(error, expected));
}

static void errors_in_english(void)
{
    char error[200];
    int line = 0;
    ui_text_set_language(UI_LANG_EN_US);
    const char *source = "void setup() {\n  pinMode(13 OUTPUT);\n}\nvoid loop() {}";
    assert(!mcu_compile(source, NULL, 0, error, sizeof(error), &line));
    assert(line == 2 && !strcmp(error, "Line 2: missing ')' before 'OUTPUT'."));
    ui_text_set_language(UI_LANG_PT_BR);
}

static void errors(void)
{
    char error[200];
    int line = 0;
    assert(!mcu_compile("void setup() {\n  int x = ;\n}\nvoid loop() {}", NULL, 0, error, sizeof(error), &line));
    assert(line == 2);
    assert_line_error(error, 2, TXT_MCU_ERROR_BAD_EXPRESSION, ";");
    assert(!mcu_compile("void setup() { foo(); }\nvoid loop() {}", NULL, 0, error, sizeof(error), &line));
    assert(strstr(error, "foo"));
    assert(!mcu_compile("void loop() {}", NULL, 0, error, sizeof(error), &line));
    McuProgram *program = mcu_compile("int a[2];\nvoid setup() { a[5] = 1; }\nvoid loop() {}", NULL, 0, error,
                                      sizeof(error), &line);
    assert(program);
    McuVm *vm = mcu_vm_create(program);
    FakeBoard board = {0};
    McuHost host = {&board, pin_mode, digital_write, digital_read, analog_read, analog_write, serial_write, lcd};
    char expected[200];
    snprintf(expected, sizeof(expected), tr(TXT_MCU_ERROR_STOPPED), tr(TXT_MCU_ERROR_ARRAY_INDEX));
    assert(!mcu_vm_run(vm, &host, 0, error, sizeof(error)) && !strcmp(error, expected));
    mcu_vm_free(vm);
    mcu_program_free(program);
}

static void random_bounds(void)
{
    FakeBoard board;
    McuHost host;
    McuProgram *program;
    McuVm *vm = start("void setup(){Serial.println(random(0.5)); Serial.println(random(0));"
                      "Serial.println(random(5,5)); Serial.println(random(5,2));"
                      "Serial.println(random(-2,-1)); Serial.println(random(2.9,3.9));"
                      "Serial.println(random(pow(10,400))); Serial.println(random(0,4294967296)>=0);}"
                      "void loop(){delay(1);}", &board, &host, &program);
    run(vm, &host, 0);
    assert(!strcmp(board.serial, "0\n0\n5\n5\n-2\n2\n0\n1\n"));
    mcu_vm_free(vm);
    mcu_program_free(program);
}

int main(void)
{
    random_bounds();
    blink();
    language();
    errors();
    errors_in_english();
    assert(mcu_completion_count() > 20);
    puts("MCU language: blink timing, loops, arrays, functions, Serial, LCD and errors passed.");
    return 0;
}
