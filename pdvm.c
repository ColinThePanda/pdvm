#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define INPUT_BUFFER_SIZE 255
#define NUM int64_t
#define NUM_FMT "%lli"

typedef struct
{
    NUM *items;
    size_t capacity;
    size_t count;
} Vm;

void vm_push(Vm *vm, NUM num)
{
    da_append(vm, num);
}

NUM vm_pop(Vm *vm)
{
    return da_pop(vm);
}

void vm_add(Vm *vm)
{
    vm_push(vm, vm_pop(vm) + vm_pop(vm));
}

void vm_sub(Vm *vm)
{
    NUM num1 = vm_pop(vm);
    NUM num2 = vm_pop(vm);
    vm_push(vm, num2 - num1);
}

void vm_mul(Vm *vm)
{
    vm_push(vm, vm_pop(vm) * vm_pop(vm));
}

void vm_div(Vm *vm)
{
    NUM num1 = vm_pop(vm);
    NUM num2 = vm_pop(vm);
    vm_push(vm, num2 - num1);
}

void vm_mod(Vm *vm)
{
    NUM num1 = vm_pop(vm);
    NUM num2 = vm_pop(vm);
    vm_push(vm, num2 % num1);
}

void vm_exp(Vm *vm)
{
    NUM num1 = vm_pop(vm);
    NUM num2 = vm_pop(vm);
    NUM result = 1;
    for (NUM i = 0; i < num1; i++)
    {
        result *= num2;
    }
    vm_push(vm, result);
}

void vm_dup(Vm *vm)
{
    vm_push(vm, da_last(vm));
}

void vm_swap(Vm *vm)
{
    NUM num1 = vm_pop(vm);
    NUM num2 = vm_pop(vm);
    vm_push(vm, num1);
    vm_push(vm, num2);
}

void vm_print(Vm *vm)
{
    printf(NUM_FMT"\n", da_last(vm));
}

void vm_dump(Vm *vm)
{
    printf("[");
    for (size_t i = 0; i < vm->count; i++)
    {
        printf(NUM_FMT, vm->items[i]);
        if (i + 1 != vm->count)
        {
            printf(", ");
        }
    }
    printf("]\n");
}

void vm_clear(Vm *vm)
{
    vm->count = 0;
}

void vm_neg(Vm *vm)
{
    vm_push(vm, -vm_pop(vm));
}

void vm_eq(Vm *vm)
{
    vm_push(vm, vm_pop(vm) == vm_pop(vm));
}

void vm_neq(Vm *vm)
{
    vm_push(vm, vm_pop(vm) != vm_pop(vm));
}

void vm_lt(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a < b);
}

void vm_gt(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a > b);
}

void vm_lte(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a <= b);
}

void vm_gte(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a >= b);
}

void vm_and(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a != 0 && b != 0);
}

void vm_or(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);
    vm_push(vm, a != 0 || b != 0);
}

void vm_not(Vm *vm)
{
    vm_push(vm, vm_pop(vm) == 0);
}

bool sv_eq_ignore_case(String_View a, const char *b)
{
    size_t b_len = strlen(b);

    if (a.count != b_len) return false;

    for (size_t i = 0; i < a.count; i++)
    {
        char ca = tolower((unsigned char)a.data[i]);
        char cb = tolower((unsigned char)b[i]);

        if (ca != cb) return false;
    }

    return true;
}

int exec_line(Vm *vm, String_View line)
{
    line = sv_trim(line);

    if (line.count == 0)
        return 1;

    if (line.data[0] == '#')
        return 1;

    String_View command = sv_chop_by_delim(&line, ' ');
    command = sv_trim(command);
    line = sv_trim(line);

    if (sv_eq_ignore_case(command, "halt"))
    {
        return 2;
    }
    else if (sv_eq_ignore_case(command, "push"))
    {
        if (line.count == 0)
        {
            printf("push requires a number\n");
            return 1;
        }

        const char *num_str = temp_sv_to_cstr(line);
        vm_push(vm, atoll(num_str));
    }
    else if (sv_eq_ignore_case(command, "pop"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_pop(vm);
    }
    else if (sv_eq_ignore_case(command, "print"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_print(vm);
    }
    else if (sv_eq_ignore_case(command, "add"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_add(vm);
    }
    else if (sv_eq_ignore_case(command, "sub"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_sub(vm);
    }
    else if (sv_eq_ignore_case(command, "mul"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_mul(vm);
    }
    else if (sv_eq_ignore_case(command, "div"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_div(vm);
    }
    else if (sv_eq_ignore_case(command, "mod"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_mod(vm);
    }
    else if (sv_eq_ignore_case(command, "exp"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_exp(vm);
    }
    else if (sv_eq_ignore_case(command, "swap"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_swap(vm);
    }
    else if (sv_eq_ignore_case(command, "dup"))
    {
        if (vm->count == 0)
        {
            printf("There needs to be at least one number on the stack\n");
            return 1;
        }

        vm_dup(vm);
    }
    else if (sv_eq_ignore_case(command, "dump"))
    {
        vm_dump(vm);
    }
    else if (sv_eq_ignore_case(command, "clear"))
    {
        vm_clear(vm);
    }
    else if (sv_eq_ignore_case(command, "neg"))
    {
        if (vm->count == 0)
        {
            printf("There needs to be at least one number on the stack\n");
            return 1;
        }

        vm_neg(vm);
    }
    else if (sv_eq_ignore_case(command, "eq"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_eq(vm);
    }
    else if (sv_eq_ignore_case(command, "neq"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_neq(vm);
    }
    else if (sv_eq_ignore_case(command, "lt"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_lt(vm);
    }
    else if (sv_eq_ignore_case(command, "gt"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_gt(vm);
    }
    else if (sv_eq_ignore_case(command, "lte"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_lte(vm);
    }
    else if (sv_eq_ignore_case(command, "gte"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_gte(vm);
    }
    else if (sv_eq_ignore_case(command, "and"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_and(vm);
    }
    else if (sv_eq_ignore_case(command, "or"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_or(vm);
    }
    else if (sv_eq_ignore_case(command, "not"))
    {
        if (vm->count == 0)
        {
            printf("There needs to be at least one number on the stack\n");
            return 1;
        }

        vm_not(vm);
    }
    else
    {
        printf("Command not found\n");
    }

    return 1;
}

int console(Vm *vm)
{
    for (;;)
    {
        char input[INPUT_BUFFER_SIZE];
        printf("vm> ");
        if (!fgets(input, sizeof(input), stdin))
            break;
        if (exec_line(vm, sv_from_cstr(input)) == 2)
            return 0;
    }
    return 0;
}

int exec_file(Vm *vm, char *filepath) {
    String_Builder file = {0};
    if (!read_entire_file(filepath, &file)) {
        printf("Failed to read file\n");
        return 1;
    }

    String_View content = sb_to_sv(file);

    while (content.count > 0) {
        String_View line = sv_chop_by_delim(&content, '\n');

        line = sv_trim(line);

        if (line.count == 0) continue;
        if (line.data[0] == '#') continue;
        if (exec_line(vm, line) == 2) break;
    }

    sb_free(file);
    return 0;
}

int main(int argc, char *argv[])
{
    Vm vm = {0};

    if (argc == 1)
    {
        return console(&vm);
    }
    else
    {
        char *file_path = argv[1];
        exec_file(&vm, file_path);
    }

    return 0;
}