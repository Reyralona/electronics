#include <assert.h>
#include "circuit.h"

int main(void)
{
    Circuit c = {0, 0};
    assert(!circuit_led_on(&c));
    c.switch_closed = 1;
    assert(!circuit_led_on(&c));
    c.source_on = 1;
    assert(circuit_led_on(&c));
    c.switch_closed = 0;
    assert(!circuit_led_on(&c));
    return 0;
}
