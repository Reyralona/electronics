#include <stdio.h>
#include "circuit.h"

int main(void)
{
    Circuit circuit = {1, 0};
    int command;

    puts("Circuito didatico fixo: fonte -> interruptor -> LED -> fonte");
    puts("Comandos: i = alternar interruptor; f = alternar fonte; q = sair");
    for (;;) {
        printf("Fonte: %s | Interruptor: %s | LED: %s\n",
               circuit.source_on ? "ligada" : "desligada",
               circuit.switch_closed ? "fechado" : "aberto",
               circuit_led_on(&circuit) ? "aceso" : "apagado");
        printf("> ");
        fflush(stdout);
        command = getchar();
        if (command == EOF || command == 'q') break;
        if (command == 'i') circuit.switch_closed = !circuit.switch_closed;
        else if (command == 'f') circuit.source_on = !circuit.source_on;
        else if (command != '\n' && command != '\r') puts("Comando desconhecido.");
        if (command != '\n') {
            while ((command = getchar()) != '\n' && command != EOF) { }
            if (command == EOF) break;
        }
    }
    return 0;
}
