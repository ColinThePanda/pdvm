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
    printf(NUM_FMT, da_last(vm));
    printf("\n");
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

int exec_line(Vm *vm, char *line)
{
    char *command = strtok(line, " \n");
    if (!command)
        return 1;

    for (NUM i = 0; command[i]; i++)
    {
        command[i] = tolower((unsigned char)command[i]);
    }

    if (strcmp(command, "halt") == 0)
    {
        return 2;
    }
    else if (strcmp(command, "push") == 0)
    {
        char *num_str = strtok(NULL, " \n");
        if (num_str)
        {
            vm_push(vm, atoi(num_str));
        }
        else
        {
            printf("push requires a number\n");
            return 1;
        }
    }
    else if (strcmp(command, "pop") == 0)
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }
        vm_pop(vm);
    }
    else if (strcmp(command, "print") == 0)
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }
        vm_print(vm);
    }
    else if (strcmp(command, "add") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_add(vm);
    }
    else if (strcmp(command, "sub") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_sub(vm);
    }
    else if (strcmp(command, "mul") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_mul(vm);
    }
    else if (strcmp(command, "div") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_div(vm);
    }
    else if (strcmp(command, "mod") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_mod(vm);
    }
    else if (strcmp(command, "exp") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_exp(vm);
    }
    else if (strcmp(command, "swap") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_swap(vm);
    }
    else if (strcmp(command, "dup") == 0)
    {
        if (vm->count == 0)
        {
            printf("There needs to at least one number on the stack\n");
            return 1;
        }
        vm_dup(vm);
    }
    else if (strcmp(command, "dump") == 0)
    {
        vm_dump(vm);
    }
    else if (strcmp(command, "clear") == 0)
    {
        vm_clear(vm);
    }
    else if (strcmp(command, "neg") == 0)
    {
        if (vm->count == 0)
        {
            printf("There needs to at least one number on the stack\n");
            return 1;
        }
        vm_neg(vm);
    }
    else if (strcmp(command, "eq") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_eq(vm);
    }
    else if (strcmp(command, "neq") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_neq(vm);
    }
    else if (strcmp(command, "lt") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_lt(vm);
    }
    else if (strcmp(command, "gt") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_gt(vm);
    }
    else if (strcmp(command, "lte") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_lte(vm);
    }
    else if (strcmp(command, "gte") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_gte(vm);
    }
    else if (strcmp(command, "and") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_and(vm);
    }
    else if (strcmp(command, "or") == 0)
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }
        vm_or(vm);
    }
    else if (strcmp(command, "not") == 0)
    {
        if (vm->count == 0)
        {
            printf("There needs to at least one number on the stack\n");
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
        if (exec_line(vm, input) == 2)
            return 0;
    }
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
        printf("Running file %s", file_path);
    }

    return 0;
}