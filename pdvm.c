#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define INPUT_BUFFER_SIZE 255
#define NUM int64_t
#define NUM_FMT "%lli"

bool parse_num(const char *input, NUM *out)
{
    return sscanf(input, NUM_FMT, out) == 1;
}

typedef struct
{
    String_View *items;
    size_t capacity;
    size_t count;
} Lines;

typedef struct
{
    String_View name;
    size_t ip;
} Label;

typedef struct
{
    Label *items;
    size_t capacity;
    size_t count;
} Labels;

typedef struct
{
    NUM *items;
    size_t count;
    size_t capacity;
} Memory;

bool memory_ensure(Memory *memory, NUM addr)
{
    if (addr < 0)
    {
        printf("Memory address out of range\n");
        return false;
    }

    size_t index = (size_t)addr;

    if (index >= memory->count)
    {
        da_resize(memory, index + 1);
    }

    return true;
}

bool label_lookup(Labels labels, String_View name, size_t *ip)
{
    for (size_t i = 0; i < labels.count; i++)
    {
        if (sv_eq(labels.items[i].name, name))
        {
            *ip = labels.items[i].ip;
            return true;
        }
    }

    return false;
}
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
    vm_push(vm, num2 / num1);
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
    printf("]");
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

void vm_over(Vm *vm)
{
    vm_push(vm, vm->items[vm->count - 2]);
}

void vm_input(Vm *vm)
{
    char input[INPUT_BUFFER_SIZE];

    printf("input> ");

    if (!fgets(input, sizeof(input), stdin))
    {
        printf("Failed to read input\n");
        return;
    }

    NUM num;

    if (!parse_num(input, &num))
    {
        printf("Input must be a number\n");
        return;
    }

    vm_push(vm, num);
}

void vm_rot(Vm *vm)
{
    NUM c = vm_pop(vm);
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);

    vm_push(vm, b);
    vm_push(vm, c);
    vm_push(vm, a);
}

bool vm_read(Vm *vm, Memory *memory)
{
    NUM addr = vm_pop(vm);

    if (!memory_ensure(memory, addr))
        return false;

    vm_push(vm, memory->items[addr]);
    return true;
}

bool vm_write(Vm *vm, Memory *memory)
{
    NUM addr = vm_pop(vm);
    NUM value = vm_pop(vm);

    if (!memory_ensure(memory, addr))
        return false;

    memory->items[addr] = value;
    return true;
}

void vm_printc(Vm *vm)
{
    printf("%c", (char)da_last(vm));
}

bool vm_jump(Labels labels, String_View label_name, size_t *ip)
{
    size_t target = 0;

    if (!label_lookup(labels, label_name, &target))
    {
        printf("Unknown label: %.*s\n", (int)label_name.count, label_name.data);
        return false;
    }

    *ip = target;
    return true;
}

bool sv_eq_ignore_case(String_View a, const char *b)
{
    size_t b_len = strlen(b);

    if (a.count != b_len)
        return false;

    for (size_t i = 0; i < a.count; i++)
    {
        char ca = tolower((unsigned char)a.data[i]);
        char cb = tolower((unsigned char)b[i]);

        if (ca != cb)
            return false;
    }

    return true;
}

int exec_line(Vm *vm, Memory *memory, bool console, String_View line)
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
        NUM num;
        if (!parse_num(num_str, &num))
        {
            return 1;
        }
        vm_push(vm, num);
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
        if (console)
            printf("\n");
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
        if (console)
            printf("\n");
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
    else if (sv_eq_ignore_case(command, "over"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_over(vm);
    }
    else if (sv_eq_ignore_case(command, "input"))
    {
        vm_input(vm);
    }
    else if (sv_eq_ignore_case(command, "rot"))
    {
        if (vm->count < 3)
        {
            printf("There is less than 3 numbers on the stack\n");
            return 1;
        }

        vm_rot(vm);
    }
    else if (sv_eq_ignore_case(command, "read"))
    {
        if (vm->count == 0)
        {
            printf("There needs to be at least one number on the stack\n");
            return 1;
        }

        if (!(vm_read(vm, memory)))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "write"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!(vm_write(vm, memory)))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "printc"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_printc(vm);
        if (console)
            printf("\n");
    }
    else
    {
        printf("Command not found\n");
    }

    return 1;
}

int exec_program(Vm *vm, String_View program)
{
    Lines lines = {0};
    Labels labels = {0};
    Memory memory = {0};

    while (program.count > 0)
    {
        String_View line = sv_chop_by_delim(&program, '\n');

        line = sv_trim(line);

        if (line.count == 0)
            continue;
        if (line.data[0] == '#')
            continue;

        da_append(&lines, line);
    }

    for (size_t i = 0; i < lines.count; i++)
    {
        String_View line = lines.items[i];
        String_View rest = line;

        String_View command = sv_chop_by_delim(&rest, ' ');
        command = sv_trim(command);
        rest = sv_trim(rest);

        if (sv_eq_ignore_case(command, "label"))
        {
            if (rest.count == 0)
            {
                printf("label requires a name\n");
                return 1;
            }

            Label label = {rest, i + 1};

            da_append(&labels, label);
        }
    }

    size_t ip = 0;

    while (ip < lines.count)
    {
        String_View line = lines.items[ip];
        String_View rest = line;

        String_View command = sv_chop_by_delim(&rest, ' ');
        command = sv_trim(command);
        rest = sv_trim(rest);

        if (sv_eq_ignore_case(command, "label"))
        {
            ip++;
        }
        else if (sv_eq_ignore_case(command, "jmp"))
        {
            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jz"))
        {
            if (vm_pop(vm) != 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jnz"))
        {
            if (vm_pop(vm) == 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jneg"))
        {
            if (vm_pop(vm) >= 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jpos"))
        {
            if (vm_pop(vm) <= 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jlez"))
        {
            if (vm_pop(vm) > 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jgez"))
        {
            if (vm_pop(vm) < 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "je"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a != b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jne"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a == b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jl"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a < b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jg"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a > b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jle"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a <= b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jge"))
        {
            if (vm->count < 2)
            {
                printf("Must have 2 numbers on the stack");
                continue;
            }

            NUM a = vm_pop(vm);
            NUM b = vm_pop(vm);

            if (!(a >= b))
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else
        {
            if (exec_line(vm, &memory, false, line) == 2)
                break;

            ip++;
        }
    }

    da_free(memory);

    return 0;
}

int console(Vm *vm)
{
    Memory memory = {0};
    for (;;)
    {
        char input[INPUT_BUFFER_SIZE];
        printf("pdvm> ");
        if (!fgets(input, sizeof(input), stdin))
            break;
        if (exec_line(vm, &memory, true, sv_from_cstr(input)) == 2)
            return 0;
    }
    return 0;
}

int exec_file(Vm *vm, char *filepath)
{
    String_Builder file = {0};
    if (!read_entire_file(filepath, &file))
    {
        printf("Failed to read file\n");
        return 1;
    }

    String_View file_content = sb_to_sv(file);

    int result = exec_program(vm, file_content);

    sb_free(file);
    return result;
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