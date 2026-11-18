/* Inspect the allocation boundary: a public runtime error alone can hide an earlier overwrite. */
#include "../src/mcu_lang.c"
#include <assert.h>

int main(void)
{
    char error[200];
    int line = 0;
    McuProgram *program = mcu_compile("void f(){int a[230]; f();} void setup(){f();} void loop(){}",
                                      NULL, 0, error, sizeof(error), &line);
    assert(program);
    McuVm *vm = mcu_vm_create(program);
    assert(vm);
    McuHost host = {0};
    for (int executed = 0; executed < VM_BUDGET; ++executed)
    {
        if (program->code[vm->pc] == OP_LOCALS &&
            vm->sp + program->code[vm->pc + 1] - program->code[vm->pc + 2] > VM_STACK)
        {
            int previous_sp = vm->sp;
            assert(!step(vm, &host, 0, error, sizeof(error)));
            assert(vm->sp == previous_sp);
            char expected[200];
            snprintf(expected, sizeof(expected), tr(TXT_MCU_ERROR_STOPPED), tr(TXT_MCU_ERROR_CALL_DEPTH));
            assert(!strcmp(error, expected));
            mcu_vm_free(vm);
            mcu_program_free(program);
            return 0;
        }
        assert(step(vm, &host, 0, error, sizeof(error)));
    }
    assert(!"recursive program did not reach the stack boundary");
    return 1;
}
