#include "mcu_lang.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_MAX_LEN 128
#define MAX_GLOBALS 1024
#define MAX_LOCALS 256
#define MAX_FUNCTIONS 64
#define MAX_DEFINES 64
#define MAX_LCDS 4
#define MAX_LOOPS 32
#define MAX_PATCHES 64
#define VM_STACK 1024
#define VM_FRAMES 64
#define VM_BUDGET 200000

/* ---------------------------------------------------------------- bytecode */
enum
{
    OP_PUSHK, OP_PUSHS, OP_LOADG, OP_STOREG, OP_LOADL, OP_STOREL, OP_LOADGA, OP_STOREGA, OP_LOADLA, OP_STORELA,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE, OP_BAND, OP_BOR, OP_BXOR,
    OP_SHL, OP_SHR, OP_NEG, OP_NOT, OP_BNOT, OP_JMP, OP_JZ, OP_JNZ, OP_CALL, OP_RET, OP_BUILTIN, OP_POP, OP_DUP,
    OP_TOINT, OP_LOCALS, OP_OVER
};

enum
{
    B_PINMODE, B_DWRITE, B_DREAD, B_AREAD, B_AWRITE, B_DELAY, B_DELAYUS, B_MILLIS, B_MICROS, B_MAP, B_CONSTRAIN,
    B_MIN, B_MAX, B_ABS, B_RANDOM, B_RANDOMSEED, B_SQRT, B_POW, B_SIN, B_COS, B_TONE, B_NOTONE, B_SBEGIN,
    B_SPRINT, B_SPRINTLN, B_LCD_BASE
};

typedef struct
{
    const char *name;
    int id, min_args, max_args;
} Builtin;

static const Builtin builtins[] = {
    {"pinMode", B_PINMODE, 2, 2},       {"digitalWrite", B_DWRITE, 2, 2},   {"digitalRead", B_DREAD, 1, 1},
    {"analogRead", B_AREAD, 1, 1},      {"analogWrite", B_AWRITE, 2, 2},    {"delay", B_DELAY, 1, 1},
    {"delayMicroseconds", B_DELAYUS, 1, 1}, {"millis", B_MILLIS, 0, 0},  {"micros", B_MICROS, 0, 0},
    {"map", B_MAP, 5, 5},               {"constrain", B_CONSTRAIN, 3, 3},   {"min", B_MIN, 2, 2},
    {"max", B_MAX, 2, 2},               {"abs", B_ABS, 1, 1},               {"random", B_RANDOM, 1, 2},
    {"randomSeed", B_RANDOMSEED, 1, 1}, {"sqrt", B_SQRT, 1, 1},             {"pow", B_POW, 2, 2},
    {"sin", B_SIN, 1, 1},               {"cos", B_COS, 1, 1},               {"tone", B_TONE, 2, 3},
    {"noTone", B_NOTONE, 1, 1},         {"Serial.begin", B_SBEGIN, 1, 1},   {"Serial.print", B_SPRINT, 1, 2},
    {"Serial.println", B_SPRINTLN, 0, 2},
};

typedef struct
{
    const char *method;
    McuLcdOperation operation;
    int min_args, max_args;
} LcdMethod;

static const LcdMethod lcd_methods[] = {
    {"init", MCU_LCD_INIT, 0, 0},   {"begin", MCU_LCD_INIT, 0, 2},        {"backlight", MCU_LCD_BACKLIGHT, 0, 0},
    {"noBacklight", MCU_LCD_NO_BACKLIGHT, 0, 0}, {"clear", MCU_LCD_CLEAR, 0, 0}, {"home", MCU_LCD_HOME, 0, 0},
    {"setCursor", MCU_LCD_SET_CURSOR, 2, 2}, {"print", MCU_LCD_PRINT, 1, 1},
};

static const McuCompletion completions[] = {
    {"pinMode", "pinMode(pino, INPUT | OUTPUT | INPUT_PULLUP)", "Define o pino como entrada ou saída"},
    {"digitalWrite", "digitalWrite(pino, HIGH | LOW)", "Liga ou desliga uma saída digital"},
    {"digitalRead", "digitalRead(pino)", "Lê HIGH/LOW de uma entrada"},
    {"analogRead", "analogRead(pino)", "Lê a tensão do pino (0–1023; ESP32 0–4095)"},
    {"analogWrite", "analogWrite(pino, 0..255)", "PWM, simulado como tensão média"},
    {"delay", "delay(ms)", "Pausa o programa"},
    {"delayMicroseconds", "delayMicroseconds(us)", "Pausa curta"},
    {"millis", "millis()", "Milissegundos desde o início"},
    {"micros", "micros()", "Microssegundos desde o início"},
    {"map", "map(valor, deMin, deMax, paraMin, paraMax)", "Converte faixas"},
    {"constrain", "constrain(valor, min, max)", "Limita um valor"},
    {"min", "min(a, b)", "Menor valor"},
    {"max", "max(a, b)", "Maior valor"},
    {"abs", "abs(x)", "Valor absoluto"},
    {"random", "random(max) | random(min, max)", "Número aleatório"},
    {"randomSeed", "randomSeed(semente)", "Semente aleatória"},
    {"sqrt", "sqrt(x)", "Raiz quadrada"},
    {"pow", "pow(base, expoente)", "Potência"},
    {"tone", "tone(pino, frequência)", "Gera tom (simulado como 50% de PWM)"},
    {"noTone", "noTone(pino)", "Para o tom"},
    {"Serial.begin", "Serial.begin(9600)", "Abre o monitor serial"},
    {"Serial.print", "Serial.print(valor)", "Escreve no monitor serial"},
    {"Serial.println", "Serial.println(valor)", "Escreve e quebra a linha"},
    {"LiquidCrystal_I2C", "LiquidCrystal_I2C lcd(0x27, 16, 2);", "Declara um LCD 16x2 I2C"},
    {"lcd.init", "lcd.init()", "Inicializa o LCD"},
    {"lcd.backlight", "lcd.backlight()", "Liga a luz de fundo"},
    {"lcd.clear", "lcd.clear()", "Apaga o LCD"},
    {"lcd.setCursor", "lcd.setCursor(coluna, linha)", "Posiciona o cursor"},
    {"lcd.print", "lcd.print(valor)", "Escreve no LCD"},
    {"setup", "void setup() { }", "Roda uma vez ao iniciar"},
    {"loop", "void loop() { }", "Roda repetidamente"},
    {"HIGH", "HIGH", "Nível alto (1)"},
    {"LOW", "LOW", "Nível baixo (0)"},
    {"OUTPUT", "OUTPUT", "Modo saída"},
    {"INPUT", "INPUT", "Modo entrada"},
    {"INPUT_PULLUP", "INPUT_PULLUP", "Entrada com pull-up interno"},
    {"LED_BUILTIN", "LED_BUILTIN", "LED da placa"},
    {"for", "for (int i = 0; i < n; i++) { }", "Repetição"},
    {"while", "while (condição) { }", "Repetição"},
    {"if", "if (condição) { } else { }", "Decisão"},
    {"int", "int nome = valor;", "Inteiro"},
    {"float", "float nome = valor;", "Número com vírgula"},
};

int mcu_completion_count(void)
{
    return (int)(sizeof(completions) / sizeof(completions[0]));
}

const McuCompletion *mcu_completion(int index)
{
    return index >= 0 && index < mcu_completion_count() ? &completions[index] : NULL;
}

struct McuProgram
{
    int *code;
    int count, capacity;
    double *constants;
    int constant_count, constant_capacity;
    char **strings;
    int string_count;
    int globals;
    int functions;
    int function_address[MAX_FUNCTIONS];
};

/* ---------------------------------------------------------------- lexer */
typedef enum
{
    T_EOF, T_NUMBER, T_STRING, T_NAME, T_PUNCT
} TokenKind;

typedef struct
{
    TokenKind kind;
    char text[128];
    double number;
    int is_float, line;
} Token;

typedef struct
{
    char name[NAME_MAX_LEN];
    int slot, size, is_int, is_array;
} Var;

typedef struct
{
    char name[NAME_MAX_LEN];
    int params, defined, returns_value;
} Function;

typedef struct
{
    char name[NAME_MAX_LEN], value[64];
} Define;

typedef struct
{
    int break_patches[MAX_PATCHES], break_count;
    int continue_patches[MAX_PATCHES], continue_count;
    int continue_target;
} Loop;

typedef struct
{
    const char *source;
    size_t position;
    int line;
    Token token;
    McuProgram *program;
    const McuConstant *constants;
    int constant_count;
    Var globals[MAX_GLOBALS / 4];
    int global_count;
    Var locals[MAX_LOCALS];
    int local_count, local_slots, max_local_slots, in_function, function_is_void;
    Function functions[MAX_FUNCTIONS];
    Define defines[MAX_DEFINES];
    int define_count;
    char lcds[MAX_LCDS][NAME_MAX_LEN];
    int lcd_count;
    Loop loops[MAX_LOOPS];
    int loop_depth;
    char *error;
    size_t error_capacity;
    int failed, error_line;
} Compiler;

static void fail(Compiler *c, const char *format, ...)
{
    if (c->failed)
        return;
    c->failed = 1;
    c->error_line = c->token.line;
    char reason[200];
    va_list args;
    va_start(args, format);
    vsnprintf(reason, sizeof(reason), format, args);
    va_end(args);
    snprintf(c->error, c->error_capacity, "linha %d: %s", c->token.line, reason);
}

static void skip_space(Compiler *c)
{
    for (;;)
    {
        char ch = c->source[c->position];
        if (ch == '\n')
            ++c->line;
        if (isspace((unsigned char)ch))
            ++c->position;
        else if (ch == '/' && c->source[c->position + 1] == '/')
            while (c->source[c->position] && c->source[c->position] != '\n')
                ++c->position;
        else if (ch == '/' && c->source[c->position + 1] == '*')
        {
            c->position += 2;
            while (c->source[c->position] && !(c->source[c->position] == '*' && c->source[c->position + 1] == '/'))
                c->line += c->source[c->position++] == '\n';
            if (c->source[c->position])
                c->position += 2;
        }
        else if (ch == '#')
        {
            size_t start = c->position;
            while (c->source[c->position] && c->source[c->position] != '\n')
                ++c->position;
            char directive[160];
            size_t length = c->position - start < sizeof(directive) - 1 ? c->position - start : sizeof(directive) - 1;
            memcpy(directive, c->source + start, length);
            directive[length] = 0;
            Define define;
            if (sscanf(directive, "#define %47s %63s", define.name, define.value) == 2 && c->define_count < MAX_DEFINES)
                c->defines[c->define_count++] = define;
        }
        else
            return;
    }
}

static void lex_number(Compiler *c, const char *text)
{
    char *end;
    c->token.kind = T_NUMBER;
    c->token.is_float = strchr(text, '.') != NULL || ((strchr(text, 'e') || strchr(text, 'E')) && strncmp(text, "0x", 2));
    c->token.number = !strncmp(text, "0x", 2) || !strncmp(text, "0X", 2) ? (double)strtoll(text, &end, 16)
                      : !strncmp(text, "0b", 2) ? (double)strtoll(text + 2, &end, 2)
                                                 : strtod(text, &end);
}

static void apply_define(Compiler *c)
{
    for (int i = 0; i < c->define_count; ++i)
        if (!strcmp(c->defines[i].name, c->token.text))
        {
            snprintf(c->token.text, sizeof(c->token.text), "%s", c->defines[i].value);
            if (isdigit((unsigned char)c->token.text[0]) || c->token.text[0] == '.')
                lex_number(c, c->token.text);
            return;
        }
}

static void next(Compiler *c)
{
    static const char *punct[] = {"<<=", ">>=", "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "==",
                                  "!=", "<=", ">=", "&&", "||", "<<", ">>"};
    skip_space(c);
    const char *s = c->source + c->position;
    c->token.line = c->line;
    c->token.text[0] = 0;
    if (!*s)
    {
        c->token.kind = T_EOF;
        return;
    }
    size_t length = 0;
    if (isalpha((unsigned char)*s) || *s == '_')
    {
        while ((isalnum((unsigned char)s[length]) || s[length] == '_') && length < sizeof(c->token.text) - 1)
            ++length;
        c->token.kind = T_NAME;
    }
    else if (isdigit((unsigned char)*s) || (*s == '.' && isdigit((unsigned char)s[1])))
    {
        while ((isalnum((unsigned char)s[length]) || s[length] == '.') && length < sizeof(c->token.text) - 1)
            ++length;
        c->token.kind = T_NUMBER;
    }
    else if (*s == '"' || *s == '\'')
    {
        char quote = *s;
        size_t out = 0;
        length = 1;
        while (s[length] && s[length] != quote && out < sizeof(c->token.text) - 1)
        {
            char ch = s[length++];
            if (ch == '\\' && s[length])
            {
                ch = s[length++];
                ch = ch == 'n' ? '\n' : ch == 't' ? '\t' : ch;
            }
            c->token.text[out++] = ch;
        }
        c->token.text[out] = 0;
        if (s[length] == quote)
            ++length;
        c->position += length;
        if (quote == '\'')
        {
            c->token.kind = T_NUMBER;
            c->token.is_float = 0;
            c->token.number = (unsigned char)c->token.text[0];
        }
        else
            c->token.kind = T_STRING;
        return;
    }
    else
    {
        length = 1;
        for (size_t i = 0; i < sizeof(punct) / sizeof(punct[0]); ++i)
            if (!strncmp(s, punct[i], strlen(punct[i])))
            {
                length = strlen(punct[i]);
                break;
            }
        c->token.kind = T_PUNCT;
    }
    memcpy(c->token.text, s, length);
    c->token.text[length] = 0;
    c->position += length;
    if (c->token.kind == T_NUMBER)
        lex_number(c, c->token.text);
    if (c->token.kind == T_NAME)
        apply_define(c);
}

static int is(Compiler *c, const char *text)
{
    return (c->token.kind == T_PUNCT || c->token.kind == T_NAME) && !strcmp(c->token.text, text);
}

static int accept(Compiler *c, const char *text)
{
    if (!is(c, text))
        return 0;
    next(c);
    return 1;
}

static void expect(Compiler *c, const char *text)
{
    if (!accept(c, text))
        fail(c, "esperado '%s' e encontrado '%s'", text, c->token.text);
}

/* ---------------------------------------------------------------- emission */
static int emit(Compiler *c, int value)
{
    McuProgram *p = c->program;
    if (p->count == p->capacity)
    {
        int capacity = p->capacity ? p->capacity * 2 : 1024;
        int *grown = realloc(p->code, (size_t)capacity * sizeof(int));
        if (!grown)
        {
            fail(c, "memória insuficiente");
            return p->count;
        }
        p->code = grown;
        p->capacity = capacity;
    }
    p->code[p->count] = value;
    return p->count++;
}

static void emit_constant(Compiler *c, double value)
{
    McuProgram *p = c->program;
    if (p->constant_count == p->constant_capacity)
    {
        int capacity = p->constant_capacity ? p->constant_capacity * 2 : 64;
        double *grown = realloc(p->constants, (size_t)capacity * sizeof(double));
        if (!grown)
        {
            fail(c, "memória insuficiente");
            return;
        }
        p->constants = grown;
        p->constant_capacity = capacity;
    }
    p->constants[p->constant_count] = value;
    emit(c, OP_PUSHK);
    emit(c, p->constant_count++);
}

static void emit_string(Compiler *c, const char *text)
{
    McuProgram *p = c->program;
    char **grown = realloc(p->strings, (size_t)(p->string_count + 1) * sizeof(char *));
    char *copy = grown ? _strdup(text) : NULL;
    if (!copy)
    {
        if (grown)
            p->strings = grown;
        fail(c, "memória insuficiente");
        return;
    }
    p->strings = grown;
    p->strings[p->string_count] = copy;
    emit(c, OP_PUSHS);
    emit(c, p->string_count++);
}

static void patch(Compiler *c, int at, int target)
{
    if (at >= 0 && at < c->program->count)
        c->program->code[at] = target;
}

static int jump(Compiler *c, int op)
{
    emit(c, op);
    return emit(c, -1);
}

/* ---------------------------------------------------------------- symbols */
static const char *type_names[] = {"void", "int", "long", "short", "byte", "char", "bool", "boolean", "float",
                                   "double", "unsigned", "signed", "const", "static", "volatile", "uint8_t",
                                   "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t", "size_t", "word"};

static int is_type(Compiler *c)
{
    if (c->token.kind != T_NAME)
        return 0;
    for (size_t i = 0; i < sizeof(type_names) / sizeof(type_names[0]); ++i)
        if (!strcmp(c->token.text, type_names[i]))
            return 1;
    return 0;
}

/* Consumes qualifiers and the base type; returns 1 for integer types, 0 for float, -1 for void. */
static int parse_type(Compiler *c)
{
    int kind = 1;
    while (is_type(c))
    {
        if (is(c, "float") || is(c, "double"))
            kind = 0;
        else if (is(c, "void"))
            kind = -1;
        next(c);
    }
    return kind;
}

static Var *find_var(Compiler *c, const char *name, int *global)
{
    for (int i = c->local_count - 1; i >= 0; --i)
        if (!strcmp(c->locals[i].name, name))
        {
            *global = 0;
            return &c->locals[i];
        }
    for (int i = 0; i < c->global_count; ++i)
        if (!strcmp(c->globals[i].name, name))
        {
            *global = 1;
            return &c->globals[i];
        }
    return NULL;
}

static int find_function(Compiler *c, const char *name, int create)
{
    for (int i = 0; i < c->program->functions; ++i)
        if (!strcmp(c->functions[i].name, name))
            return i;
    if (!create || c->program->functions == MAX_FUNCTIONS)
        return -1;
    int index = c->program->functions++;
    snprintf(c->functions[index].name, NAME_MAX_LEN, "%s", name);
    c->functions[index].params = -1;
    return index;
}

static int find_lcd(Compiler *c, const char *name)
{
    for (int i = 0; i < c->lcd_count; ++i)
        if (!strcmp(c->lcds[i], name))
            return i;
    return -1;
}

static int constant_value(Compiler *c, const char *name, double *value)
{
    static const McuConstant builtin_constants[] = {{"HIGH", 1}, {"LOW", 0}, {"INPUT", MCU_INPUT},
                                                    {"OUTPUT", MCU_OUTPUT}, {"INPUT_PULLUP", MCU_INPUT_PULLUP},
                                                    {"true", 1}, {"false", 0}, {"DEC", 10}, {"HEX", 16},
                                                    {"BIN", 2}, {"PI", 3.14159265358979}};
    for (size_t i = 0; i < sizeof(builtin_constants) / sizeof(builtin_constants[0]); ++i)
        if (!strcmp(builtin_constants[i].name, name))
        {
            *value = builtin_constants[i].value;
            return 1;
        }
    for (int i = 0; i < c->constant_count; ++i)
        if (!strcmp(c->constants[i].name, name))
        {
            *value = c->constants[i].value;
            return 1;
        }
    return 0;
}

/* ---------------------------------------------------------------- expressions */
typedef struct
{
    int kind; /* 0 value on stack, 1 variable, 2 array element (index on stack) */
    Var var;
    int global;
} LValue;

static void expression(Compiler *c);
static LValue unary(Compiler *c);

static void load(Compiler *c, const LValue *lv)
{
    if (lv->kind == 1)
    {
        emit(c, lv->global ? OP_LOADG : OP_LOADL);
        emit(c, lv->var.slot);
    }
    else if (lv->kind == 2)
    {
        emit(c, lv->global ? OP_LOADGA : OP_LOADLA);
        emit(c, lv->var.slot);
        emit(c, lv->var.size);
    }
}

/* Stores the value on top of the stack (array: index below it) and leaves the stored value. */
static void store(Compiler *c, const LValue *lv)
{
    if (lv->var.is_int)
        emit(c, OP_TOINT);
    if (lv->kind == 1)
    {
        emit(c, lv->global ? OP_STOREG : OP_STOREL);
        emit(c, lv->var.slot);
    }
    else
    {
        emit(c, lv->global ? OP_STOREGA : OP_STORELA);
        emit(c, lv->var.slot);
        emit(c, lv->var.size);
    }
}

static void value(Compiler *c, LValue lv)
{
    load(c, &lv);
}

static int call_arguments(Compiler *c)
{
    int count = 0;
    expect(c, "(");
    if (!is(c, ")"))
        do
        {
            if (c->token.kind == T_STRING)
            {
                emit_string(c, c->token.text);
                next(c);
            }
            else
                expression(c);
            ++count;
        } while (accept(c, ","));
    expect(c, ")");
    return count;
}

static void builtin_call(Compiler *c, const Builtin *builtin)
{
    int count = call_arguments(c);
    if (count < builtin->min_args || count > builtin->max_args)
        fail(c, "%s recebe %d a %d argumentos", builtin->name, builtin->min_args, builtin->max_args);
    emit(c, OP_BUILTIN);
    emit(c, builtin->id);
    emit(c, count);
}

static int member_call(Compiler *c, const char *object)
{
    char member[NAME_MAX_LEN], full[NAME_MAX_LEN * 2];
    snprintf(member, sizeof(member), "%s", c->token.text);
    next(c);
    snprintf(full, sizeof(full), "%s.%s", object, member);
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i)
        if (!strcmp(builtins[i].name, full))
        {
            builtin_call(c, &builtins[i]);
            return 1;
        }
    int lcd = find_lcd(c, object);
    if (lcd < 0)
    {
        fail(c, "objeto desconhecido '%s'", object);
        return 0;
    }
    for (size_t i = 0; i < sizeof(lcd_methods) / sizeof(lcd_methods[0]); ++i)
        if (!strcmp(lcd_methods[i].method, member))
        {
            emit_constant(c, lcd);
            int count = call_arguments(c);
            if (count < lcd_methods[i].min_args || count > lcd_methods[i].max_args)
                fail(c, "%s.%s: número de argumentos inválido", object, member);
            emit(c, OP_BUILTIN);
            emit(c, B_LCD_BASE + (int)lcd_methods[i].operation);
            emit(c, count + 1);
            return 1;
        }
    fail(c, "método desconhecido '%s'", full);
    return 0;
}

static LValue name_reference(Compiler *c)
{
    LValue lv = {0};
    char name[NAME_MAX_LEN];
    snprintf(name, sizeof(name), "%s", c->token.text);
    next(c);
    if (accept(c, "."))
    {
        member_call(c, name);
        return lv;
    }
    if (is(c, "("))
    {
        for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i)
            if (!strcmp(builtins[i].name, name))
            {
                builtin_call(c, &builtins[i]);
                return lv;
            }
        int function = find_function(c, name, 1);
        if (function < 0)
        {
            fail(c, "funções demais");
            return lv;
        }
        int count = call_arguments(c);
        emit(c, OP_CALL);
        emit(c, function);
        emit(c, count);
        return lv;
    }
    double constant;
    int global;
    Var *var = find_var(c, name, &global);
    if (!var)
    {
        if (constant_value(c, name, &constant))
            emit_constant(c, constant);
        else
            fail(c, "nome desconhecido '%s'", name);
        return lv;
    }
    lv.var = *var;
    lv.global = global;
    lv.kind = 1;
    if (var->is_array && accept(c, "["))
    {
        expression(c);
        expect(c, "]");
        lv.kind = 2;
    }
    return lv;
}

static LValue primary(Compiler *c)
{
    LValue lv = {0};
    if (c->token.kind == T_NUMBER)
    {
        emit_constant(c, c->token.number);
        if (!c->token.is_float)
            emit(c, OP_TOINT);
        next(c);
    }
    else if (c->token.kind == T_STRING)
    {
        emit_string(c, c->token.text);
        next(c);
    }
    else if (c->token.kind == T_NAME)
        lv = name_reference(c);
    else if (accept(c, "("))
    {
        if (is_type(c))
        {
            int is_int = parse_type(c);
            expect(c, ")");
            value(c, unary(c));
            if (is_int > 0)
                emit(c, OP_TOINT);
            return lv;
        }
        expression(c);
        expect(c, ")");
    }
    else
        fail(c, "expressão inválida perto de '%s'", c->token.text);
    return lv;
}

static void increment(Compiler *c, LValue lv, int delta, int postfix)
{
    if (lv.kind == 0)
    {
        fail(c, "++/-- precisa de uma variável");
        return;
    }
    if (lv.kind == 2)
        emit(c, OP_DUP);
    load(c, &lv);
    emit_constant(c, 1);
    emit(c, delta > 0 ? OP_ADD : OP_SUB);
    store(c, &lv);
    if (postfix)
    {
        emit_constant(c, 1);
        emit(c, delta > 0 ? OP_SUB : OP_ADD);
    }
}

static LValue postfix(Compiler *c)
{
    LValue lv = primary(c);
    if (is(c, "++") || is(c, "--"))
    {
        int delta = is(c, "++") ? 1 : -1;
        next(c);
        increment(c, lv, delta, 1);
        lv.kind = 0;
    }
    return lv;
}

static LValue unary(Compiler *c)
{
    LValue none = {0};
    if (is(c, "++") || is(c, "--"))
    {
        int delta = is(c, "++") ? 1 : -1;
        next(c);
        increment(c, unary(c), delta, 0);
        return none;
    }
    if (accept(c, "-"))
    {
        value(c, unary(c));
        emit(c, OP_NEG);
        return none;
    }
    if (accept(c, "+"))
    {
        value(c, unary(c));
        return none;
    }
    if (accept(c, "!"))
    {
        value(c, unary(c));
        emit(c, OP_NOT);
        return none;
    }
    if (accept(c, "~"))
    {
        value(c, unary(c));
        emit(c, OP_BNOT);
        return none;
    }
    return postfix(c);
}

typedef struct
{
    const char *text;
    int op, precedence;
} BinaryOp;

static const BinaryOp binary_ops[] = {
    {"|", OP_BOR, 3},  {"^", OP_BXOR, 4}, {"&", OP_BAND, 5}, {"==", OP_EQ, 6}, {"!=", OP_NE, 6},
    {"<", OP_LT, 7},   {"<=", OP_LE, 7},  {">", OP_GT, 7},   {">=", OP_GE, 7}, {"<<", OP_SHL, 8},
    {">>", OP_SHR, 8}, {"+", OP_ADD, 9},  {"-", OP_SUB, 9},  {"*", OP_MUL, 10}, {"/", OP_DIV, 10},
    {"%", OP_MOD, 10},
};

static const BinaryOp *binary_op(Compiler *c)
{
    if (c->token.kind != T_PUNCT)
        return NULL;
    for (size_t i = 0; i < sizeof(binary_ops) / sizeof(binary_ops[0]); ++i)
        if (!strcmp(binary_ops[i].text, c->token.text))
            return &binary_ops[i];
    return NULL;
}

static void binary(Compiler *c, int min_precedence)
{
    value(c, unary(c));
    for (;;)
    {
        const BinaryOp *op = binary_op(c);
        if (!op || op->precedence < min_precedence || c->failed)
            return;
        next(c);
        binary(c, op->precedence + 1);
        emit(c, op->op);
    }
}

static void logical_and(Compiler *c)
{
    binary(c, 3);
    if (!is(c, "&&"))
        return;
    int false_patches[MAX_PATCHES], count = 0;
    false_patches[count++] = jump(c, OP_JZ);
    while (accept(c, "&&") && count < MAX_PATCHES)
    {
        binary(c, 3);
        false_patches[count++] = jump(c, OP_JZ);
    }
    emit_constant(c, 1);
    int end = jump(c, OP_JMP);
    for (int i = 0; i < count; ++i)
        patch(c, false_patches[i], c->program->count);
    emit_constant(c, 0);
    patch(c, end, c->program->count);
}

static void logical_or(Compiler *c)
{
    logical_and(c);
    if (!is(c, "||"))
        return;
    int true_patches[MAX_PATCHES], count = 0;
    true_patches[count++] = jump(c, OP_JNZ);
    while (accept(c, "||") && count < MAX_PATCHES)
    {
        logical_and(c);
        true_patches[count++] = jump(c, OP_JNZ);
    }
    emit_constant(c, 0);
    int end = jump(c, OP_JMP);
    for (int i = 0; i < count; ++i)
        patch(c, true_patches[i], c->program->count);
    emit_constant(c, 1);
    patch(c, end, c->program->count);
}

static void ternary(Compiler *c)
{
    logical_or(c);
    if (!accept(c, "?"))
        return;
    int otherwise = jump(c, OP_JZ);
    expression(c);
    int end = jump(c, OP_JMP);
    expect(c, ":");
    patch(c, otherwise, c->program->count);
    ternary(c);
    patch(c, end, c->program->count);
}

static int compound_op(Compiler *c)
{
    static const struct
    {
        const char *text;
        int op;
    } ops[] = {{"+=", OP_ADD}, {"-=", OP_SUB}, {"*=", OP_MUL}, {"/=", OP_DIV}, {"%=", OP_MOD},
               {"&=", OP_BAND}, {"|=", OP_BOR}, {"^=", OP_BXOR}, {"<<=", OP_SHL}, {">>=", OP_SHR}};
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i)
        if (is(c, ops[i].text))
            return ops[i].op;
    return -1;
}

/* Assignment needs the target before its value: parse a unary target first and rewind when it
 * turns out to be an ordinary expression. */
static void expression(Compiler *c)
{
    size_t position = c->position;
    int line = c->line, code = c->program->count, constants = c->program->constant_count;
    Token token = c->token;
    LValue lv = unary(c);
    int op = compound_op(c);
    if (lv.kind && (is(c, "=") || op >= 0))
    {
        next(c);
        if (op >= 0)
        {
            if (lv.kind == 2)
                emit(c, OP_DUP);
            load(c, &lv);
            expression(c);
            emit(c, op);
        }
        else
            expression(c);
        store(c, &lv);
        return;
    }
    c->position = position;
    c->line = line;
    c->token = token;
    c->program->count = code;
    c->program->constant_count = constants;
    ternary(c);
}

/* ---------------------------------------------------------------- statements */
static void statement(Compiler *c);

static Var *declare(Compiler *c, const char *name, int is_int, int size)
{
    Var *table = c->in_function ? c->locals : c->globals;
    int *count = c->in_function ? &c->local_count : &c->global_count;
    int limit = c->in_function ? MAX_LOCALS : MAX_GLOBALS / 4;
    if (*count >= limit)
    {
        fail(c, "variáveis demais");
        return NULL;
    }
    Var *var = &table[(*count)++];
    snprintf(var->name, NAME_MAX_LEN, "%s", name);
    var->is_int = is_int;
    var->is_array = size > 0;
    var->size = size > 0 ? size : 1;
    if (c->in_function)
    {
        var->slot = c->local_slots;
        c->local_slots += var->size;
        if (c->local_slots > c->max_local_slots)
            c->max_local_slots = c->local_slots;
    }
    else
    {
        var->slot = c->program->globals;
        c->program->globals += var->size;
        if (c->program->globals > MAX_GLOBALS)
            fail(c, "memória de variáveis esgotada");
    }
    return var;
}

static void array_initializer(Compiler *c, const Var *var)
{
    LValue lv = {2, *var, !c->in_function};
    int index = 0;
    expect(c, "{");
    if (!is(c, "}"))
        do
        {
            if (index >= var->size)
            {
                fail(c, "inicializadores demais para '%s'", var->name);
                return;
            }
            emit_constant(c, index++);
            expression(c);
            store(c, &lv);
            emit(c, OP_POP);
        } while (accept(c, ","));
    expect(c, "}");
}

static void declaration_rest(Compiler *c, int is_int, char *first_name)
{
    char name[NAME_MAX_LEN];
    snprintf(name, sizeof(name), "%s", first_name);
    for (;;)
    {
        int size = 0;
        if (accept(c, "["))
        {
            if (c->token.kind == T_NUMBER)
            {
                size = (int)c->token.number;
                next(c);
            }
            expect(c, "]");
            if (size <= 0 && !is(c, "="))
                fail(c, "tamanho do vetor inválido");
            if (size <= 0)
                size = 64;
        }
        Var *var = declare(c, name, is_int, size);
        if (!var)
            return;
        if (accept(c, "="))
        {
            if (var->is_array)
                array_initializer(c, var);
            else
            {
                LValue lv = {1, *var, !c->in_function};
                expression(c);
                store(c, &lv);
                emit(c, OP_POP);
            }
        }
        if (!accept(c, ","))
            break;
        snprintf(name, sizeof(name), "%s", c->token.text);
        next(c);
    }
    expect(c, ";");
}

static void block(Compiler *c)
{
    int locals = c->local_count;
    expect(c, "{");
    while (!is(c, "}") && c->token.kind != T_EOF && !c->failed)
        statement(c);
    expect(c, "}");
    c->local_count = locals;
}

static void loop_begin(Compiler *c, int continue_target)
{
    if (c->loop_depth == MAX_LOOPS)
    {
        fail(c, "laços aninhados demais");
        return;
    }
    Loop *loop = &c->loops[c->loop_depth++];
    memset(loop, 0, sizeof(*loop));
    loop->continue_target = continue_target;
}

static void loop_end(Compiler *c, int continue_target, int break_target)
{
    Loop *loop = &c->loops[--c->loop_depth];
    for (int i = 0; i < loop->break_count; ++i)
        patch(c, loop->break_patches[i], break_target);
    for (int i = 0; i < loop->continue_count; ++i)
        patch(c, loop->continue_patches[i], continue_target);
}

static void if_statement(Compiler *c)
{
    expect(c, "(");
    expression(c);
    expect(c, ")");
    int otherwise = jump(c, OP_JZ);
    statement(c);
    if (accept(c, "else"))
    {
        int end = jump(c, OP_JMP);
        patch(c, otherwise, c->program->count);
        statement(c);
        patch(c, end, c->program->count);
    }
    else
        patch(c, otherwise, c->program->count);
}

static void while_statement(Compiler *c)
{
    int start = c->program->count;
    expect(c, "(");
    expression(c);
    expect(c, ")");
    int exit = jump(c, OP_JZ);
    loop_begin(c, start);
    statement(c);
    emit(c, OP_JMP);
    emit(c, start);
    patch(c, exit, c->program->count);
    loop_end(c, start, c->program->count);
}

static void do_statement(Compiler *c)
{
    int start = c->program->count;
    loop_begin(c, -1);
    statement(c);
    int condition = c->program->count;
    expect(c, "while");
    expect(c, "(");
    expression(c);
    expect(c, ")");
    expect(c, ";");
    emit(c, OP_JNZ);
    emit(c, start);
    loop_end(c, condition, c->program->count);
}

/* for: the step is written before the body but runs after it, so its tokens are revisited. */
static void for_statement(Compiler *c)
{
    int locals = c->local_count;
    expect(c, "(");
    if (is_type(c))
    {
        int is_int = parse_type(c);
        char name[NAME_MAX_LEN];
        snprintf(name, sizeof(name), "%s", c->token.text);
        next(c);
        declaration_rest(c, is_int > 0, name);
    }
    else
    {
        if (!is(c, ";"))
        {
            expression(c);
            emit(c, OP_POP);
        }
        expect(c, ";");
    }
    int condition = c->program->count, exit = -1;
    if (!is(c, ";"))
    {
        expression(c);
        exit = jump(c, OP_JZ);
    }
    expect(c, ";");
    size_t step_position = c->position;
    int step_line = c->line;
    Token step_token = c->token;
    int depth = 0;
    while (c->token.kind != T_EOF && !(depth == 0 && is(c, ")")))
    {
        depth += is(c, "(") - is(c, ")");
        next(c);
    }
    expect(c, ")");
    loop_begin(c, -1);
    statement(c);
    size_t after_position = c->position;
    int after_line = c->line;
    Token after_token = c->token;
    int step = c->program->count;
    c->position = step_position;
    c->line = step_line;
    c->token = step_token;
    if (!is(c, ")"))
    {
        expression(c);
        emit(c, OP_POP);
    }
    c->position = after_position;
    c->line = after_line;
    c->token = after_token;
    emit(c, OP_JMP);
    emit(c, condition);
    if (exit >= 0)
        patch(c, exit, c->program->count);
    loop_end(c, step, c->program->count);
    c->local_count = locals;
}

static void jump_statement(Compiler *c, int is_break)
{
    if (!c->loop_depth)
    {
        fail(c, "%s fora de um laço", is_break ? "break" : "continue");
        return;
    }
    Loop *loop = &c->loops[c->loop_depth - 1];
    int at = jump(c, OP_JMP);
    if (is_break && loop->break_count < MAX_PATCHES)
        loop->break_patches[loop->break_count++] = at;
    else if (!is_break && loop->continue_count < MAX_PATCHES)
        loop->continue_patches[loop->continue_count++] = at;
    expect(c, ";");
}

static void statement(Compiler *c)
{
    if (c->failed)
        return;
    if (is(c, "{"))
        block(c);
    else if (accept(c, "if"))
        if_statement(c);
    else if (accept(c, "while"))
        while_statement(c);
    else if (accept(c, "do"))
        do_statement(c);
    else if (accept(c, "for"))
        for_statement(c);
    else if (accept(c, "break"))
        jump_statement(c, 1);
    else if (accept(c, "continue"))
        jump_statement(c, 0);
    else if (accept(c, "return"))
    {
        if (is(c, ";"))
            emit_constant(c, 0);
        else
            expression(c);
        expect(c, ";");
        emit(c, OP_RET);
    }
    else if (is_type(c))
    {
        int is_int = parse_type(c);
        char name[NAME_MAX_LEN];
        snprintf(name, sizeof(name), "%s", c->token.text);
        next(c);
        declaration_rest(c, is_int > 0, name);
    }
    else if (accept(c, ";"))
        return;
    else
    {
        expression(c);
        emit(c, OP_POP);
        expect(c, ";");
    }
}

static void function_definition(Compiler *c, const char *name, int returns_value)
{
    int index = find_function(c, name, 1);
    if (index < 0 || c->functions[index].defined)
    {
        fail(c, index < 0 ? "funções demais" : "função '%s' definida duas vezes", name);
        return;
    }
    int skip = jump(c, OP_JMP);
    c->program->function_address[index] = c->program->count;
    c->in_function = 1;
    c->local_count = c->local_slots = c->max_local_slots = 0;
    expect(c, "(");
    int params = 0;
    if (!is(c, ")") && !(is(c, "void") && c->source[c->position] != 0))
        do
        {
            int is_int = parse_type(c);
            char param[NAME_MAX_LEN];
            snprintf(param, sizeof(param), "%s", c->token.text);
            next(c);
            declare(c, param, is_int > 0, 0);
            ++params;
        } while (accept(c, ","));
    else
        accept(c, "void");
    expect(c, ")");
    c->functions[index].params = params;
    c->functions[index].defined = 1;
    c->functions[index].returns_value = returns_value;
    emit(c, OP_LOCALS);
    int locals_at = emit(c, 0);
    emit(c, params);
    block(c);
    emit_constant(c, 0);
    emit(c, OP_RET);
    patch(c, locals_at, c->max_local_slots);
    c->in_function = 0;
    c->local_count = 0;
    patch(c, skip, c->program->count);
}

static void top_level(Compiler *c)
{
    if (accept(c, ";"))
        return;
    if (accept(c, "LiquidCrystal_I2C"))
    {
        if (c->lcd_count == MAX_LCDS)
        {
            fail(c, "LCDs demais");
            return;
        }
        snprintf(c->lcds[c->lcd_count++], NAME_MAX_LEN, "%s", c->token.text);
        next(c);
        if (accept(c, "("))
            while (!accept(c, ")") && c->token.kind != T_EOF)
                next(c);
        expect(c, ";");
        return;
    }
    if (!is_type(c))
    {
        fail(c, "declaração esperada, encontrado '%s'", c->token.text);
        return;
    }
    int kind = parse_type(c);
    char name[NAME_MAX_LEN];
    snprintf(name, sizeof(name), "%s", c->token.text);
    if (c->token.kind != T_NAME)
    {
        fail(c, "nome esperado");
        return;
    }
    next(c);
    if (is(c, "("))
        function_definition(c, name, kind >= 0);
    else
        declaration_rest(c, kind > 0, name);
}

static void entry_stub(Compiler *c)
{
    int setup = find_function(c, "setup", 0), loop = find_function(c, "loop", 0);
    if (setup < 0 || !c->functions[setup].defined || loop < 0 || !c->functions[loop].defined)
    {
        fail(c, "o programa precisa de void setup() e void loop()");
        return;
    }
    emit(c, OP_CALL);
    emit(c, setup);
    emit(c, 0);
    emit(c, OP_POP);
    int again = c->program->count;
    emit(c, OP_CALL);
    emit(c, loop);
    emit(c, 0);
    emit(c, OP_POP);
    emit(c, OP_JMP);
    emit(c, again);
}

static void check_calls(Compiler *c)
{
    for (int i = 0; i < c->program->functions && !c->failed; ++i)
        if (!c->functions[i].defined)
            fail(c, "função '%s' não definida", c->functions[i].name);
}

McuProgram *mcu_compile(const char *source, const McuConstant *constants, int constant_count, char *error,
                        size_t capacity, int *error_line)
{
    Compiler *c = calloc(1, sizeof(*c));
    McuProgram *program = calloc(1, sizeof(*program));
    if (!c || !program || !source)
    {
        free(c);
        free(program);
        snprintf(error, capacity, "memória insuficiente");
        return NULL;
    }
    c->source = source;
    c->line = 1;
    c->program = program;
    c->constants = constants;
    c->constant_count = constant_count;
    c->error = error;
    c->error_capacity = capacity;
    next(c);
    while (c->token.kind != T_EOF && !c->failed)
        top_level(c);
    if (!c->failed)
    {
        check_calls(c);
        entry_stub(c);
    }
    if (c->failed)
    {
        if (error_line)
            *error_line = c->error_line;
        mcu_program_free(program);
        program = NULL;
    }
    free(c);
    return program;
}

void mcu_program_free(McuProgram *program)
{
    if (!program)
        return;
    for (int i = 0; i < program->string_count; ++i)
        free(program->strings[i]);
    free(program->strings);
    free(program->code);
    free(program->constants);
    free(program);
}

/* ---------------------------------------------------------------- virtual machine */
typedef struct
{
    double v;
    int kind; /* 0 int, 1 float, 2 string index */
} Value;

typedef struct
{
    int return_pc, base;
} Frame;

struct McuVm
{
    const McuProgram *program;
    Value *globals;
    Value stack[VM_STACK];
    int sp, pc, fp;
    Frame frames[VM_FRAMES];
    double wake_ms;
    int stopped;
    unsigned random_state;
};

McuVm *mcu_vm_create(const McuProgram *program)
{
    McuVm *vm = calloc(1, sizeof(*vm));
    if (!vm || !program)
    {
        free(vm);
        return NULL;
    }
    vm->program = program;
    vm->globals = calloc((size_t)(program->globals ? program->globals : 1), sizeof(Value));
    if (!vm->globals)
    {
        free(vm);
        return NULL;
    }
    vm->random_state = 12345;
    return vm;
}

void mcu_vm_free(McuVm *vm)
{
    if (!vm)
        return;
    free(vm->globals);
    free(vm);
}

static Value make(double v, int is_int)
{
    Value value = {is_int ? trunc(v) : v, is_int ? 0 : 1};
    return value;
}

static void format_value(const McuVm *vm, Value value, int base, char *text, size_t capacity)
{
    if (value.kind == 2)
        snprintf(text, capacity, "%s", vm->program->strings[(int)value.v]);
    else if (value.kind == 1)
        snprintf(text, capacity, "%.2f", value.v);
    else if (base == 16)
        snprintf(text, capacity, "%llX", (unsigned long long)(long long)value.v);
    else if (base == 2)
    {
        unsigned long long bits = (unsigned long long)(long long)value.v;
        size_t used = 0;
        int started = 0;
        for (int bit = 63; bit >= 0 && used + 1 < capacity; --bit)
            if ((bits >> bit & 1) || started || bit == 0)
            {
                started = 1;
                text[used++] = (bits >> bit & 1) ? '1' : '0';
            }
        text[used] = 0;
    }
    else
        snprintf(text, capacity, "%lld", (long long)value.v);
}

static int vm_error(McuVm *vm, char *error, size_t capacity, const char *message)
{
    vm->stopped = 1;
    snprintf(error, capacity, "Erro de execução: %s", message);
    return 0;
}

static Value builtin(McuVm *vm, const McuHost *host, int id, Value *a, int count, double now_ms)
{
    char text[160];
    switch (id)
    {
    case B_PINMODE:
        host->pin_mode(host->context, (int)a[0].v, (int)a[1].v);
        break;
    case B_DWRITE:
        host->digital_write(host->context, (int)a[0].v, a[1].v != 0);
        break;
    case B_DREAD:
        return make(host->digital_read(host->context, (int)a[0].v), 1);
    case B_AREAD:
        return make(host->analog_read(host->context, (int)a[0].v), 1);
    case B_AWRITE:
        host->analog_write(host->context, (int)a[0].v, (int)fmax(0, fmin(255, a[1].v)));
        break;
    case B_DELAY:
        vm->wake_ms = now_ms + fmax(0, a[0].v);
        break;
    case B_DELAYUS:
        vm->wake_ms = now_ms + fmax(0, a[0].v) / 1000;
        break;
    case B_MILLIS:
        return make(floor(now_ms), 1);
    case B_MICROS:
        return make(floor(now_ms * 1000), 1);
    case B_MAP:
        if (a[2].v == a[1].v)
            return make(a[3].v, 1);
        return make((a[0].v - a[1].v) * (a[4].v - a[3].v) / (a[2].v - a[1].v) + a[3].v, a[0].kind == 0);
    case B_CONSTRAIN:
        return make(fmax(a[1].v, fmin(a[2].v, a[0].v)), a[0].kind == 0);
    case B_MIN:
        return make(fmin(a[0].v, a[1].v), a[0].kind == 0 && a[1].kind == 0);
    case B_MAX:
        return make(fmax(a[0].v, a[1].v), a[0].kind == 0 && a[1].kind == 0);
    case B_ABS:
        return make(fabs(a[0].v), a[0].kind == 0);
    case B_RANDOM:
    {
        vm->random_state = vm->random_state * 1103515245u + 12345u;
        double low = count == 2 ? a[0].v : 0, high = count == 2 ? a[1].v : a[0].v;
        double span = high - low;
        return make(span > 0 ? low + (vm->random_state >> 8) % (unsigned)span : low, 1);
    }
    case B_RANDOMSEED:
        vm->random_state = (unsigned)a[0].v;
        break;
    case B_SQRT:
        return make(sqrt(fmax(0, a[0].v)), 0);
    case B_POW:
        return make(pow(a[0].v, a[1].v), 0);
    case B_SIN:
        return make(sin(a[0].v), 0);
    case B_COS:
        return make(cos(a[0].v), 0);
    case B_TONE:
        host->analog_write(host->context, (int)a[0].v, 128);
        break;
    case B_NOTONE:
        host->analog_write(host->context, (int)a[0].v, 0);
        break;
    case B_SBEGIN:
        break;
    case B_SPRINT:
    case B_SPRINTLN:
        text[0] = 0;
        if (count > 0)
            format_value(vm, a[0], count > 1 ? (int)a[1].v : 10, text, sizeof(text) - 2);
        if (id == B_SPRINTLN)
            strcat(text, "\n");
        host->serial_write(host->context, text);
        break;
    default:
        if (id >= B_LCD_BASE)
        {
            McuLcdOperation operation = (McuLcdOperation)(id - B_LCD_BASE);
            text[0] = 0;
            if (operation == MCU_LCD_PRINT)
                format_value(vm, a[1], 10, text, sizeof(text));
            host->lcd(host->context, (int)a[0].v, operation, count > 1 ? (int)a[1].v : 0,
                      count > 2 ? (int)a[2].v : 0, text);
        }
    }
    return make(0, 1);
}

static Value arithmetic(int op, Value a, Value b)
{
    int ints = a.kind == 0 && b.kind == 0;
    long long x = (long long)a.v, y = (long long)b.v;
    switch (op)
    {
    case OP_ADD:
        return make(a.v + b.v, ints);
    case OP_SUB:
        return make(a.v - b.v, ints);
    case OP_MUL:
        return make(a.v * b.v, ints);
    case OP_DIV:
        if (ints)
            return make(y ? (double)(x / y) : 0, 1);
        return make(b.v != 0 ? a.v / b.v : 0, 0);
    case OP_MOD:
        return make(y ? (double)(x % y) : 0, 1);
    case OP_LT:
        return make(a.v < b.v, 1);
    case OP_LE:
        return make(a.v <= b.v, 1);
    case OP_GT:
        return make(a.v > b.v, 1);
    case OP_GE:
        return make(a.v >= b.v, 1);
    case OP_EQ:
        return make(a.v == b.v, 1);
    case OP_NE:
        return make(a.v != b.v, 1);
    case OP_BAND:
        return make((double)(x & y), 1);
    case OP_BOR:
        return make((double)(x | y), 1);
    case OP_BXOR:
        return make((double)(x ^ y), 1);
    case OP_SHL:
        return make((double)(x << (y & 63)), 1);
    default:
        return make((double)(x >> (y & 63)), 1);
    }
}

#define POP() (vm->stack[--vm->sp])
#define PUSH(value) (vm->stack[vm->sp++] = (value))

static int step(McuVm *vm, const McuHost *host, double now_ms, char *error, size_t capacity)
{
    const int *code = vm->program->code;
    int op = code[vm->pc++];
    if (vm->sp > VM_STACK - 16)
        return vm_error(vm, error, capacity, "pilha esgotada (recursão profunda?)");
    switch (op)
    {
    case OP_PUSHK:
        PUSH(make(vm->program->constants[code[vm->pc]], 0));
        vm->stack[vm->sp - 1].kind = 1;
        ++vm->pc;
        break;
    case OP_PUSHS:
        PUSH(((Value){code[vm->pc++], 2}));
        break;
    case OP_LOADG:
        PUSH(vm->globals[code[vm->pc++]]);
        break;
    case OP_STOREG:
        vm->globals[code[vm->pc++]] = vm->stack[vm->sp - 1];
        break;
    case OP_LOADL:
        PUSH(vm->stack[vm->fp + code[vm->pc++]]);
        break;
    case OP_STOREL:
        vm->stack[vm->fp + code[vm->pc++]] = vm->stack[vm->sp - 1];
        break;
    case OP_LOADGA:
    case OP_LOADLA:
    {
        int base = code[vm->pc++], size = code[vm->pc++], index = (int)POP().v;
        if (index < 0 || index >= size)
            return vm_error(vm, error, capacity, "índice fora do vetor");
        PUSH(op == OP_LOADGA ? vm->globals[base + index] : vm->stack[vm->fp + base + index]);
        break;
    }
    case OP_STOREGA:
    case OP_STORELA:
    {
        int base = code[vm->pc++], size = code[vm->pc++];
        Value stored = POP();
        int index = (int)POP().v;
        if (index < 0 || index >= size)
            return vm_error(vm, error, capacity, "índice fora do vetor");
        if (op == OP_STOREGA)
            vm->globals[base + index] = stored;
        else
            vm->stack[vm->fp + base + index] = stored;
        PUSH(stored);
        break;
    }
    case OP_NEG:
        vm->stack[vm->sp - 1].v = -vm->stack[vm->sp - 1].v;
        break;
    case OP_NOT:
        vm->stack[vm->sp - 1] = make(vm->stack[vm->sp - 1].v == 0, 1);
        break;
    case OP_BNOT:
        vm->stack[vm->sp - 1] = make((double)~(long long)vm->stack[vm->sp - 1].v, 1);
        break;
    case OP_JMP:
        vm->pc = code[vm->pc];
        break;
    case OP_JZ:
    case OP_JNZ:
    {
        int target = code[vm->pc++];
        int zero = POP().v == 0;
        if (zero == (op == OP_JZ))
            vm->pc = target;
        break;
    }
    case OP_CALL:
    {
        int function = code[vm->pc++], argc = code[vm->pc++];
        if (vm->fp >= 0 && (vm->sp >= VM_STACK - 64))
            return vm_error(vm, error, capacity, "pilha esgotada");
        int depth = 0;
        while (depth < VM_FRAMES && vm->frames[depth].return_pc)
            ++depth;
        if (depth == VM_FRAMES)
            return vm_error(vm, error, capacity, "chamadas aninhadas demais");
        vm->frames[depth] = (Frame){vm->pc, vm->fp};
        vm->fp = vm->sp - argc;
        vm->pc = vm->program->function_address[function];
        break;
    }
    case OP_LOCALS:
    {
        int slots = code[vm->pc++], params = code[vm->pc++];
        for (int i = params; i < slots; ++i)
            PUSH(make(0, 1));
        break;
    }
    case OP_RET:
    {
        Value result = POP();
        int depth = 0;
        while (depth < VM_FRAMES && vm->frames[depth].return_pc)
            ++depth;
        if (depth == 0)
            return vm_error(vm, error, capacity, "retorno sem chamada");
        Frame frame = vm->frames[depth - 1];
        vm->frames[depth - 1].return_pc = 0;
        vm->sp = vm->fp;
        vm->fp = frame.base;
        vm->pc = frame.return_pc;
        PUSH(result);
        break;
    }
    case OP_BUILTIN:
    {
        int id = code[vm->pc++], argc = code[vm->pc++];
        Value args[8];
        for (int i = argc - 1; i >= 0; --i)
            args[i] = POP();
        PUSH(builtin(vm, host, id, args, argc, now_ms));
        break;
    }
    case OP_POP:
        --vm->sp;
        break;
    case OP_DUP:
    case OP_OVER:
    {
        Value copy = vm->stack[vm->sp - (op == OP_DUP ? 1 : 2)];
        PUSH(copy);
        break;
    }
    case OP_TOINT:
        if (vm->stack[vm->sp - 1].kind != 2)
            vm->stack[vm->sp - 1] = make(vm->stack[vm->sp - 1].v, 1);
        break;
    default:
    {
        Value b = POP(), a = POP();
        PUSH(arithmetic(op, a, b));
    }
    }
    return 1;
}

int mcu_vm_run(McuVm *vm, const McuHost *host, double now_ms, char *error, size_t capacity)
{
    if (!vm || vm->stopped)
        return 0;
    for (int executed = 0; executed < VM_BUDGET; ++executed)
    {
        if (vm->wake_ms > now_ms)
            return 1;
        if (vm->pc < 0 || vm->pc >= vm->program->count)
            return vm_error(vm, error, capacity, "programa terminou de forma inesperada");
        if (!step(vm, host, now_ms, error, capacity))
            return 0;
    }
    return 1;
}
