#include "workbench_examples.h"
#include "component_devices.h"
#include <string.h>

/* Wire colors follow the palette: 0 blue, 1 red, 2 black, 3 green, 4 yellow, 5 white. */
#define WIRE_SIGNAL 0
#define WIRE_SUPPLY 1
#define WIRE_RETURN 2
#define WIRE_CONTROL 3
#define CHAIN_SOURCE_X 330.0f
#define CHAIN_FIRST_X 520.0f
#define CHAIN_PITCH 180.0f
#define SUPPLY_LANE 120.0f
#define RETURN_LANE 150.0f

typedef int (*ExampleBuilder)(Workbench *bench, int *selected);

typedef struct
{
    const char *title, *category, *description;
    ExampleBuilder build;
} ExampleInfo;

static int connect(Workbench *b, Pin a, Pin c, int color, const WirePoint *knees, int count)
{
    if (count < 0 || count > WIRE_MAX_KNEES || bench_toggle_wire(b, a, c) != 1)
        return 0;
    Wire *wire = &b->wires[b->wire_count - 1];
    wire->color = color;
    wire->knee_count = count;
    if (count)
        memcpy(wire->knees, knees, (size_t)count * sizeof(*knees));
    return 1;
}

static int wire(Workbench *b, int part_a, int pin_a, int part_b, int pin_b, int color)
{
    return connect(b, (Pin){part_a, pin_a}, (Pin){part_b, pin_b}, color, NULL, 0);
}

/* Routes along a horizontal lane: down/up from the first terminal, across, then to the second. */
static int wire_lane(Workbench *b, int part_a, int pin_a, int part_b, int pin_b, int color, float lane)
{
    float ax, ay, bx, by;
    bench_pin_position(&b->parts[part_a], pin_a, &ax, &ay);
    bench_pin_position(&b->parts[part_b], pin_b, &bx, &by);
    WirePoint knees[2] = {{ax, lane}, {bx, lane}};
    return connect(b, (Pin){part_a, pin_a}, (Pin){part_b, pin_b}, color, knees, 2);
}

static int add_valued(Workbench *b, PartType type, float x, float y, double value)
{
    int index = bench_add(b, type, x, y);
    if (index >= 0 && value > 0 && !bench_set_value(b, index, value))
        return -1;
    return index;
}

/* Supply (part 0) feeding two-terminal parts in series, returning to the supply under the row. */
static int chain(Workbench *b, PartType supply, const PartType *types, const double *values, int count, float y)
{
    if (bench_add(b, supply, CHAIN_SOURCE_X, y) != 0)
        return 0;
    for (int i = 0; i < count; ++i)
        if (add_valued(b, types[i], CHAIN_FIRST_X + i * CHAIN_PITCH, y, values[i]) != i + 1)
            return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SUPPLY, y - SUPPLY_LANE))
        return 0;
    for (int i = 1; i < count; ++i)
        if (!wire(b, i, 1, i + 1, 0, WIRE_SIGNAL))
            return 0;
    return wire_lane(b, count, 1, 0, 1, WIRE_RETURN, y + RETURN_LANE);
}

static int build_breadboard_led(Workbench *b, int *selected)
{
    WirePoint supply[] = {{291, 150}, {587, 150}}, return_path[] = {{1019, 570}, {389, 570}},
              signal[] = {{780, 300}, {921, 300}};
    if (bench_add(b, PART_SOURCE, 340, 210) != 0 || bench_add(b, PART_BREADBOARD, 650, 400) != 1 ||
        bench_add(b, PART_RESISTOR, 638, 347) != 2 || !bench_snap_to_breadboard(b, 2) ||
        bench_add(b, PART_LED, 970, 250) != 3)
        return 0;
    if (!connect(b, (Pin){0, 0}, (Pin){1, 4}, WIRE_SUPPLY, supply, 2) ||
        !connect(b, (Pin){1, 39}, (Pin){3, 0}, 3, signal, 2) ||
        !connect(b, (Pin){3, 1}, (Pin){0, 1}, WIRE_RETURN, return_path, 2))
        return 0;
    *selected = 3;
    return 1;
}

static int build_flip_flop(Workbench *b, int *selected)
{
    WirePoint common[] = {{409, 260}, {409, 350}}, data[] = {{311, 425}, {571, 425}}, clock[] = {{311, 130}, {571, 130}},
              back[] = {{1019, 560}, {409, 560}};
    if (bench_add(b, PART_CLOCK, 360, 210) != 0 || bench_add(b, PART_CLOCK, 360, 350) != 1 ||
        !bench_set_value(b, 1, 400) || bench_add(b, PART_DFF, 620, 280) != 2 ||
        bench_add(b, PART_RESISTOR, 790, 280) != 3 || bench_add(b, PART_LED, 970, 280) != 4)
        return 0;
    if (!connect(b, (Pin){0, 1}, (Pin){1, 1}, WIRE_RETURN, common, 2) ||
        !connect(b, (Pin){1, 0}, (Pin){2, 0}, 3, data, 2) || !connect(b, (Pin){0, 0}, (Pin){2, 1}, 1, clock, 2) ||
        !wire(b, 2, 2, 3, 0, WIRE_SIGNAL) || !wire(b, 3, 1, 4, 0, WIRE_SIGNAL) ||
        !connect(b, (Pin){4, 1}, (Pin){0, 1}, WIRE_RETURN, back, 2))
        return 0;
    *selected = 2;
    return 1;
}

static int build_nand_chip(Workbench *b, int *selected)
{
    WirePoint supply[] = {{291, 115}, {447, 115}}, ground[] = {{389, 590}, {447, 590}},
              clk[] = {{291, 520}, {560, 520}, {560, 385}}, vcc[] = {{560, 415}, {560, 302}},
              gnd[] = {{730, 385}, {730, 478}}, output[] = {{720, 350}, {801, 350}}, ret[] = {{1099, 560}, {769, 560}};
    static const int unused[] = {3, 4, 8, 9, 11, 12};
    if (bench_add(b, PART_SOURCE, 340, 170) != 0 || bench_add(b, PART_BREADBOARD, 650, 400) != 1 ||
        bench_add(b, PART_74HC00, 650, 400) != 2 || !bench_snap_to_breadboard(b, 2) ||
        bench_add(b, PART_CLOCK, 340, 550) != 3 || bench_add(b, PART_RESISTOR, 850, 250) != 4 ||
        bench_add(b, PART_LED, 1050, 250) != 5)
        return 0;
    if (!connect(b, (Pin){0, 0}, (Pin){1, 300}, 1, supply, 2) || !connect(b, (Pin){0, 1}, (Pin){1, 360}, 2, ground, 2) ||
        !connect(b, (Pin){3, 1}, (Pin){1, 361}, 2, NULL, 0) || !connect(b, (Pin){3, 0}, (Pin){2, 0}, 3, clk, 3) ||
        !connect(b, (Pin){2, 1}, (Pin){1, 312}, 1, NULL, 0) || !connect(b, (Pin){2, 13}, (Pin){1, 311}, 1, vcc, 2) ||
        !connect(b, (Pin){2, 6}, (Pin){1, 377}, 2, gnd, 2))
        return 0;
    for (int k = 0; k < 6; ++k)
    {
        int pin = unused[k], column = pin < 7 ? pin : 13 - pin, rail = (pin < 7 ? 300 : 360) + 11 + column;
        if (!connect(b, (Pin){2, pin}, (Pin){1, rail}, pin < 7 ? 1 : 2, NULL, 0))
            return 0;
    }
    if (!connect(b, (Pin){2, 2}, (Pin){4, 0}, 0, output, 2) || !wire(b, 4, 1, 5, 0, WIRE_SIGNAL) ||
        !connect(b, (Pin){5, 1}, (Pin){1, 383}, 2, ret, 2))
        return 0;
    *selected = 2;
    return 1;
}

static int build_dimmer(Workbench *b, int *selected)
{
    const float y = 360;
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || bench_add(b, PART_POTENTIOMETER, 560, y) != 1 ||
        add_valued(b, PART_RESISTOR, 700, y + 160, 220) != 2 || bench_add(b, PART_LED, 880, y + 160) != 3)
        return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SUPPLY, y - SUPPLY_LANE) || !wire_lane(b, 1, 2, 0, 1, WIRE_RETURN, y - 70) ||
        !wire(b, 1, 1, 2, 0, WIRE_SIGNAL) || !wire(b, 2, 1, 3, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 3, 1, 0, 1, WIRE_RETURN, y + 260))
        return 0;
    *selected = 1;
    return 1;
}

static int build_button_led(Workbench *b, int *selected)
{
    static const PartType types[] = {PART_BUTTON, PART_RESISTOR, PART_LED};
    static const double values[] = {0, 330, 0};
    *selected = 1;
    return chain(b, PART_SOURCE, types, values, 3, 400);
}

static int build_capacitor_hold(Workbench *b, int *selected)
{
    const float y = 380;
    static const PartType types[] = {PART_SWITCH, PART_RESISTOR, PART_RESISTOR, PART_LED};
    static const double values[] = {0, 470, 1000, 0};
    if (!chain(b, PART_SOURCE, types, values, 4, y) || !bench_set_on(b, 1, 1))
        return 0;
    int capacitor = add_valued(b, PART_CAPACITOR, 790, y + 120, 2.2e-6);
    if (capacitor != 5 || !wire_lane(b, 2, 1, capacitor, 0, WIRE_SIGNAL, y + 60) ||
        !wire_lane(b, capacitor, 1, 0, 1, WIRE_RETURN, y + RETURN_LANE))
        return 0;
    *selected = 1;
    return 1;
}

static int build_series_parallel(Workbench *b, int *selected)
{
    const float y = 300, branch = 520;
    static const PartType types[] = {PART_RESISTOR, PART_LED, PART_LED};
    static const double values[] = {100, 0, 0};
    if (!chain(b, PART_SOURCE, types, values, 3, y))
        return 0;
    for (int i = 0; i < 2; ++i)
    {
        float row = branch + i * 80;
        int resistor = add_valued(b, PART_RESISTOR, CHAIN_FIRST_X, row, 220), led = bench_add(b, PART_LED, 700, row);
        if (resistor < 0 || led < 0 || !wire_lane(b, 0, 0, resistor, 0, WIRE_SUPPLY, row) ||
            !wire(b, resistor, 1, led, 0, WIRE_SIGNAL) || !wire_lane(b, led, 1, 0, 1, WIRE_RETURN, row + 40))
            return 0;
    }
    *selected = 2;
    return 1;
}

static int build_diode_polarity(Workbench *b, int *selected)
{
    const float rows[2] = {340, 500};
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, 420) != 0)
        return 0;
    for (int i = 0; i < 2; ++i)
    {
        int resistor = add_valued(b, PART_RESISTOR, CHAIN_FIRST_X, rows[i], 330);
        int diode = bench_add(b, PART_DIODE, 700, rows[i]), led = bench_add(b, PART_LED, 880, rows[i]);
        if (resistor < 0 || diode < 0 || led < 0)
            return 0;
        int diode_in = i ? 1 : 0;
        if (!wire_lane(b, 0, 0, resistor, 0, WIRE_SUPPLY, rows[i] - 60) || !wire(b, resistor, 1, diode, diode_in, 0) ||
            !wire(b, diode, 1 - diode_in, led, 0, WIRE_SIGNAL) || !wire_lane(b, led, 1, 0, 1, WIRE_RETURN, rows[i] + 60))
            return 0;
    }
    *selected = 2;
    return 1;
}

static int build_battery_led(Workbench *b, int *selected)
{
    static const PartType types[] = {PART_RESISTOR, PART_LED};
    static const double values[] = {470, 0};
    *selected = 2;
    return chain(b, PART_BATTERY_9V, types, values, 2, 400);
}

static int build_transistor_switch(Workbench *b, int *selected)
{
    const float y = 340;
    static const PartType types[] = {PART_RESISTOR, PART_LED};
    static const double values[] = {330, 0};
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || add_valued(b, types[0], CHAIN_FIRST_X, y, values[0]) != 1 ||
        bench_add(b, types[1], 700, y) != 2 || bench_add(b, PART_NPN, 880, y + 60) != 3 ||
        bench_add(b, PART_BUTTON, CHAIN_FIRST_X, y + 190) != 4 || add_valued(b, PART_RESISTOR, 700, y + 190, 10000) != 5)
        return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SUPPLY, y - SUPPLY_LANE) || !wire(b, 1, 1, 2, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 2, 1, 3, 2, WIRE_SIGNAL, y) || !wire_lane(b, 0, 0, 4, 0, WIRE_SUPPLY, y + 190) ||
        !wire(b, 4, 1, 5, 0, WIRE_CONTROL) || !wire_lane(b, 5, 1, 3, 1, WIRE_CONTROL, y + 190) ||
        !wire_lane(b, 3, 0, 0, 1, WIRE_RETURN, y + 280))
        return 0;
    *selected = 4;
    return 1;
}

static int build_light_sensor(Workbench *b, int *selected)
{
    const float y = 330, divider_y = 560, base_lane = 600, return_lane = 640;
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || add_valued(b, PART_RESISTOR, CHAIN_FIRST_X, y, 330) != 1 ||
        bench_add(b, PART_LED, 700, y) != 2 || bench_add(b, PART_NPN, 900, y + 60) != 3 ||
        bench_add(b, PART_LDR, 600, divider_y - 90) != 4 || add_valued(b, PART_RESISTOR, 760, divider_y, 4700) != 5)
        return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SUPPLY, y - SUPPLY_LANE) || !wire(b, 1, 1, 2, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 2, 1, 3, 2, WIRE_SIGNAL, y) || !wire_lane(b, 0, 0, 4, 0, WIRE_SUPPLY, base_lane - 70) ||
        !wire_lane(b, 4, 1, 3, 1, WIRE_CONTROL, base_lane) || !wire_lane(b, 5, 0, 4, 1, WIRE_CONTROL, divider_y) ||
        !wire_lane(b, 5, 1, 0, 1, WIRE_RETURN, return_lane) || !wire_lane(b, 3, 0, 0, 1, WIRE_RETURN, return_lane))
        return 0;
    *selected = 4;
    return 1;
}

static int build_inductor_delay(Workbench *b, int *selected)
{
    const float y = 380;
    static const PartType types[] = {PART_SWITCH, PART_INDUCTOR, PART_RESISTOR, PART_LED};
    static const double values[] = {0, 2, 1000, 0};
    if (!chain(b, PART_SOURCE, types, values, 4, y) || !bench_set_on(b, 1, 1))
        return 0;
    int diode = bench_add(b, PART_DIODE, 700, y + 110);
    if (diode != 5 || !wire_lane(b, diode, 0, 2, 1, WIRE_SIGNAL, y + 60) ||
        !wire_lane(b, diode, 1, 2, 0, WIRE_SIGNAL, y + 60))
        return 0;
    *selected = 2;
    return 1;
}

static int build_and_buttons(Workbench *b, int *selected)
{
    const float y = 420, inputs[2] = {320, 500};
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || bench_add(b, PART_AND, 820, y) != 1 ||
        add_valued(b, PART_RESISTOR, 990, y, 220) != 2 || bench_add(b, PART_LED, 1150, y) != 3)
        return 0;
    for (int i = 0; i < 2; ++i)
    {
        int button = bench_add(b, PART_BUTTON, CHAIN_FIRST_X, inputs[i]);
        int pull = add_valued(b, PART_RESISTOR, 650, inputs[i] + 90, 10000);
        if (button < 0 || pull < 0 || !wire_lane(b, 0, 0, button, 0, WIRE_SUPPLY, inputs[i]) ||
            !wire_lane(b, button, 1, 1, i, WIRE_CONTROL, inputs[i]) || !wire_lane(b, pull, 0, 1, i, WIRE_CONTROL, inputs[i]) ||
            !wire_lane(b, pull, 1, 0, 1, WIRE_RETURN, 630))
            return 0;
    }
    if (!wire(b, 1, 2, 2, 0, WIRE_SIGNAL) || !wire(b, 2, 1, 3, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 3, 1, 0, 1, WIRE_RETURN, 630))
        return 0;
    *selected = 1;
    return 1;
}

static int build_blinker(Workbench *b, int *selected)
{
    static const PartType types[] = {PART_RESISTOR, PART_LED};
    static const double values[] = {220, 0};
    *selected = 0;
    return chain(b, PART_CLOCK, types, values, 2, 400);
}

static int build_not_alternate(Workbench *b, int *selected)
{
    const float rows[2] = {340, 500};
    if (bench_add(b, PART_CLOCK, CHAIN_SOURCE_X, 420) != 0 || bench_add(b, PART_NOT, CHAIN_FIRST_X, rows[0]) != 1)
        return 0;
    for (int i = 0; i < 2; ++i)
    {
        int resistor = add_valued(b, PART_RESISTOR, 720, rows[i], 220), led = bench_add(b, PART_LED, 900, rows[i]);
        if (resistor < 0 || led < 0 || !wire(b, resistor, 1, led, 0, WIRE_SIGNAL) ||
            !wire_lane(b, led, 1, 0, 1, WIRE_RETURN, rows[i] + 70))
            return 0;
    }
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_CONTROL, rows[0]) || !wire(b, 1, 1, 2, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 0, 0, 4, 0, WIRE_CONTROL, rows[1]))
        return 0;
    *selected = 1;
    return 1;
}

static int build_generator_scope(Workbench *b, int *selected)
{
    const float y = 380;
    if (add_valued(b, PART_FUNCTION_GENERATOR, CHAIN_SOURCE_X, y, 1000) != 0 ||
        add_valued(b, PART_RESISTOR, CHAIN_FIRST_X, y, 220) != 1 || bench_add(b, PART_LED, 700, y) != 2 ||
        bench_add(b, PART_OSCILLOSCOPE, 600, y + 160) != 3)
        return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SIGNAL, y - SUPPLY_LANE) || !wire(b, 1, 1, 2, 0, WIRE_SIGNAL) ||
        !wire_lane(b, 2, 1, 0, 1, WIRE_RETURN, y + 90) || !wire_lane(b, 3, 0, 1, 0, WIRE_CONTROL, y + 280) ||
        !wire_lane(b, 3, 1, 0, 1, WIRE_RETURN, y + 260))
        return 0;
    *selected = 3;
    return 1;
}

static int build_relay_motor(Workbench *b, int *selected)
{
    const float y = 360;
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || bench_add(b, PART_SWITCH, CHAIN_FIRST_X, y) != 1 ||
        bench_add(b, PART_RELAY, 700, y) != 2 || bench_add(b, PART_DC_MOTOR, 900, y + 150) != 3)
        return 0;
    if (!wire_lane(b, 0, 0, 1, 0, WIRE_SUPPLY, y - SUPPLY_LANE) || !wire_lane(b, 1, 1, 2, 0, WIRE_CONTROL, y + 90) ||
        !wire_lane(b, 2, 1, 0, 1, WIRE_RETURN, y + 260) || !wire_lane(b, 0, 0, 2, 2, WIRE_SUPPLY, y - SUPPLY_LANE - 30) ||
        !wire_lane(b, 2, 3, 3, 0, WIRE_SIGNAL, y + 110) || !wire_lane(b, 3, 1, 0, 1, WIRE_RETURN, y + 260))
        return 0;
    *selected = 2;
    return 1;
}

static int build_dip_display(Workbench *b, int *selected)
{
    const float y = 300;
    static const int segment_pins[COMPONENT_DIP_POSITIONS] = {0, 1, 2, 3};
    if (bench_add(b, PART_SOURCE, CHAIN_SOURCE_X, y) != 0 || bench_add(b, PART_DIP_SWITCH, CHAIN_FIRST_X, y) != 1 ||
        bench_add(b, PART_SEVEN_SEGMENT, 900, y + 40) != 2 || !bench_set_states(b, 1, 0x3))
        return 0;
    for (int contact = 0; contact < COMPONENT_DIP_POSITIONS; ++contact)
    {
        int resistor = add_valued(b, PART_RESISTOR, 700, y + 120 + contact * 50.0f, 330);
        if (resistor < 0 || !wire_lane(b, 0, 0, 1, contact, WIRE_SUPPLY, y - SUPPLY_LANE + contact * 6) ||
            !wire_lane(b, 1, contact + 4, resistor, 0, WIRE_SIGNAL, y + 120 + contact * 50.0f) ||
            !wire_lane(b, resistor, 1, 2, segment_pins[contact], WIRE_SIGNAL, y + 120 + contact * 50.0f))
            return 0;
    }
    if (!wire_lane(b, 2, 4, 0, 1, WIRE_RETURN, y + 340))
        return 0;
    *selected = 1;
    return 1;
}

static const ExampleInfo examples[] = {
    {"LED na breadboard", "Básico", "Fonte, resistor encaixado na breadboard e LED.", build_breadboard_led},
    {"Flip-flop D", "Digital", "Dois clocks: o LED mostra Q, que só muda na borda do clock.", build_flip_flop},
    {"Porta NAND 74HC00", "Digital", "CI na breadboard inverte o clock e pisca o LED.", build_nand_chip},
    {"Dimmer com potenciômetro", "Básico", "Gire o cursor (Shift+scroll) e veja o brilho mudar.", build_dimmer},
    {"Botão acende LED", "Básico", "Segure o botão com o mouse para fechar o circuito.", build_button_led},
    {"Capacitor segura o LED", "Básico", "Abra o interruptor: o capacitor descarrega e o LED apaga aos poucos.",
     build_capacitor_hold},
    {"LEDs em série e em paralelo", "Básico", "Compare correntes e brilho nas duas ligações.", build_series_parallel},
    {"Diodo direto e reverso", "Semicondutores", "Só o ramo com o diodo polarizado diretamente acende.",
     build_diode_polarity},
    {"Bateria de 9 V", "Básico", "Bateria com resistência interna; resistor maior limita a corrente.",
     build_battery_led},
    {"Transistor como chave", "Semicondutores", "O botão injeta corrente na base e o NPN acende o LED.",
     build_transistor_switch},
    {"Sensor de luz com LDR", "Sensores", "Mais luz no LDR (slider) liga o transistor e o LED.", build_light_sensor},
    {"Indutor atrasa o LED", "Básico", "O LED acende devagar; o diodo protege ao abrir o interruptor.",
     build_inductor_delay},
    {"Porta AND com botões", "Digital", "O LED só acende com os dois botões pressionados.", build_and_buttons},
    {"Pisca-pisca com clock", "Digital", "Um clock liga e desliga o LED periodicamente.", build_blinker},
    {"NOT alterna dois LEDs", "Digital", "Um LED segue o clock e o outro o inverso.", build_not_alternate},
    {"Gerador no osciloscópio", "Medição", "Onda senoidal de 1 kHz no LED e na tela do osciloscópio.", build_generator_scope},
    {"Relé liga o motor", "Saídas", "O interruptor energiza a bobina; o contato NA liga o motor.", build_relay_motor},
    {"Chave DIP no display", "Saídas", "Cada alavanca acende um segmento (a-d). Duplo clique alterna.", build_dip_display},
};

_Static_assert(sizeof(examples) / sizeof(examples[0]) == WORKBENCH_EXAMPLE_COUNT, "example table size");

const char *workbench_example_name(int index)
{
    return index >= 0 && index < WORKBENCH_EXAMPLE_COUNT ? examples[index].title : NULL;
}

const char *workbench_example_category(int index)
{
    return index >= 0 && index < WORKBENCH_EXAMPLE_COUNT ? examples[index].category : NULL;
}

const char *workbench_example_description(int index)
{
    return index >= 0 && index < WORKBENCH_EXAMPLE_COUNT ? examples[index].description : NULL;
}

int workbench_example_create(int index, Workbench *destination, int *selected)
{
    Workbench b = {0};
    int selection = 0, published = 0;
    if (destination && selected && index >= 0 && index < WORKBENCH_EXAMPLE_COUNT &&
        examples[index].build(&b, &selection))
        published = bench_replace(destination, &b);
    if (published)
        *selected = selection;
    bench_snapshot_clear(&b);
    return published;
}
