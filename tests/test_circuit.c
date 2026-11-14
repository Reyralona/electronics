#include <assert.h>
#include "circuit.h"

int main(void)
{
    Circuit c = {0, 0, 0};
    assert(!circuit_led_on(&c));
    c.switch_closed = 1;
    assert(!circuit_led_on(&c));
    c.source_on = 1;
    assert(circuit_led_on(&c));
    for (unsigned int wire = 0; wire < 3; ++wire) {
        c.disconnected_wires = 1u << wire;
        assert(!circuit_led_on(&c));
    }
    c.disconnected_wires = 0;
    c.switch_closed = 0;
    assert(!circuit_led_on(&c));
    for (int first = 0; first < 6; ++first) {
        for (int second = 0; second < 6; ++second) {
            int expected = first != second && first / 2 == second / 2 ? first / 2 : -1;
            assert(circuit_wire_for_terminals(first, second) == expected);
        }
    }
    assert(circuit_wire_for_terminals(-1, 0) == -1);
    assert(circuit_wire_for_terminals(0, 6) == -1);
    return 0;
}
