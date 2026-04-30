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

typedef struct
{
    NUM *items;
    size_t count;
    size_t capacity;
} CallStack;

typedef struct
{
    size_t *items;
    size_t capacity;
    size_t count;
} Includes;

typedef struct
{
    String_View name;
    NUM value;
} Const;

typedef struct
{
    Const *items;
    size_t capacity;
    size_t count;
} Consts;

typedef struct
{
    size_t *items;
    size_t capacity;
    size_t count;
} ConstLineNums;

String_View sv_dup_owned(String_View sv)
{
    char *data = malloc(sv.count);

    if (data == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    memcpy(data, sv.data, sv.count);

    return sv_from_parts(data, sv.count);
}

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

void vm_pushs(Vm *vm, const char *str)
{
    size_t len = strlen(str);

    for (size_t i = 0; i < len; i++)
    {
        vm_push(vm, (NUM)str[i]);
    }

    vm_push(vm, (NUM)len);
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

void vm_prints(Vm *vm)
{
    NUM str_len = vm_pop(vm);

    if (str_len < 0 || (size_t)str_len > vm->count)
    {
        printf("Invalid string length\n");
        return;
    }

    size_t start = vm->count - (size_t)str_len;

    for (size_t i = start; i < vm->count; i++)
    {
        fputc((unsigned char)vm->items[i], stdout);
    }
}

void vm_emit(Vm *vm)
{
    NUM num = vm_pop(vm);
    printf(NUM_FMT, num);
}

void vm_emitc(Vm *vm)
{
    NUM num = vm_pop(vm);
    printf("%c", (char)num);
}

bool vm_emits(Vm *vm)
{
    NUM str_len = vm_pop(vm);

    if (str_len < 0 || (size_t)str_len > vm->count)
    {
        printf("Invalid string length\n");
        return false;
    }

    size_t len = (size_t)str_len;
    size_t start = vm->count - len;

    for (size_t i = start; i < vm->count; i++)
    {
        fputc((unsigned char)vm->items[i], stdout);
    }

    vm->count = start;
    return true;
}

void vm_dup2(Vm *vm)
{
    NUM num2 = vm->items[vm->count - 1];
    NUM num1 = vm->items[vm->count - 2];
    vm_push(vm, num1);
    vm_push(vm, num2);
}

void vm_nip(Vm *vm)
{
    NUM top = vm_pop(vm);
    vm_pop(vm);
    vm_push(vm, top);
}

void vm_tuck(Vm *vm)
{
    NUM b = vm_pop(vm);
    NUM a = vm_pop(vm);

    vm_push(vm, b);
    vm_push(vm, a);
    vm_push(vm, b);
}

void vm_depth(Vm *vm)
{
    vm_push(vm, (NUM)vm->count);
}

void vm_nl()
{
    fputc('\n', stdout);
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


bool const_lookup(Consts *consts, String_View name, NUM *value)
{
    for (size_t i = 0; i < consts->count; i++)
    {
        if (sv_eq(consts->items[i].name, name))
        {
            *value = consts->items[i].value;
            return true;
        }
    }

    return false;
}

bool const_define(Consts *consts, String_View name, NUM value)
{
    NUM existing_value;

    if (const_lookup(consts, name, &existing_value))
    {
        printf("Constant already exists: %.*s\n", (int)name.count, name.data);
        return false;
    }

    Const constant = {
        .name = sv_dup_owned(name),
        .value = value,
    };

    da_append(consts, constant);
    return true;
}

void consts_free(Consts consts)
{
    for (size_t i = 0; i < consts.count; i++)
    {
        free((void *)consts.items[i].name.data);
    }

    da_free(consts);
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

int exec_line(Vm *vm, Memory *memory, Consts *consts, bool console, String_View line)
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
            printf("push requires a number or constant\n");
            return 1;
        }

        const char *num_str = temp_sv_to_cstr(line);
        NUM num;

        if (parse_num(num_str, &num))
        {
            vm_push(vm, num);
        }
        else if (const_lookup(consts, line, &num))
        {
            vm_push(vm, num);
        }
        else
        {
            printf("Invalid number or unknown constant: %.*s\n", (int)line.count, line.data);
            return 1;
        }
    }
    else if (sv_eq_ignore_case(command, "pushs"))
    {
        if (line.count == 0)
        {
            printf("push requires a number\n");
            return 1;
        }

        const char *str = temp_sv_to_cstr(line);
        vm_pushs(vm, str);
    }
    else if (console && sv_eq_ignore_case(command, "const"))
    {
        String_View name = sv_chop_by_delim(&line, ' ');
        name = sv_trim(name);
        line = sv_trim(line);
        NUM num;

        if (name.count == 0 || line.count == 0)
        {
            printf("const requires a name and value\n");
            return 1;
        }

        if (!parse_num(temp_sv_to_cstr(line), &num))
        {
            printf("const value must be a number: %.*s\n", (int)line.count, line.data);
            return 1;
        }

        if (!const_define(consts, name, num))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "pop") || sv_eq_ignore_case(command, "drop"))
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
    else if (sv_eq_ignore_case(command, "prints"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_prints(vm);
        if (console)
            printf("\n");
    }
    else if (sv_eq_ignore_case(command, "emit"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_emit(vm);
        if (console)
            printf("\n");
    }
    else if (sv_eq_ignore_case(command, "emitc"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        vm_emitc(vm);
        if (console)
            printf("\n");
    }
    else if (sv_eq_ignore_case(command, "emits"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        if (!vm_emits(vm))
            return 1;

        if (console)
            printf("\n");
    }
    else if (sv_eq_ignore_case(command, "dup2"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_dup2(vm);
    }
    else if (sv_eq_ignore_case(command, "nip"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_nip(vm);
    }
    else if (sv_eq_ignore_case(command, "tuck"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_tuck(vm);
    }
    else if (sv_eq_ignore_case(command, "depth"))
    {
        vm_depth(vm);
    }
    else if (sv_eq_ignore_case(command, "nl"))
    {
        vm_nl();
    }
    else
    {
        printf("Command not found\n");
    }

    return 1;
}

int exec_program(Vm *vm, String_View program, Consts *consts)
{
    Lines lines = {0};
    Labels labels = {0};
    Memory memory = {0};
    CallStack call_stack = {0};

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
    if (!label_lookup(labels, sv_from_cstr("_main"), &ip))
    {
        printf("Missing entry point: label _main\n");
        return 1;
    }

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
        else if (sv_eq_ignore_case(command, "call"))
        {
            da_append(&call_stack, ip + 1);

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "ret"))
        {
            if (call_stack.count == 0)
            {
                printf("Call stack is empty\n");
                return 1;
            }

            ip = da_pop(&call_stack);
        }
        else
        {
            if (exec_line(vm, &memory, consts, false, line) == 2)
                break;

            ip++;
        }
    }

    da_free(memory);
    da_free(call_stack);

    return 0;
}

bool path_is_absolute_sv(String_View path)
{
    if (path.count == 0)
        return false;

    if (path.data[0] == '/' || path.data[0] == '\\')
        return true;

#ifdef _WIN32
    if (path.count >= 3 &&
        isalpha((unsigned char)path.data[0]) &&
        path.data[1] == ':' &&
        (path.data[2] == '/' || path.data[2] == '\\'))
    {
        return true;
    }
#endif

    return false;
}

void make_include_path(const char *source_filepath, String_View include_name, String_Builder *out)
{
    if (path_is_absolute_sv(include_name))
    {
        sb_append_sv(out, include_name);
        sb_append_null(out);
        return;
    }

    const char *dir = temp_dir_name(source_filepath);

    if (dir == NULL || dir[0] == '\0')
    {
        dir = ".";
    }

    sb_append_cstr(out, dir);

    if (out->count > 0 &&
        out->items[out->count - 1] != '/' &&
        out->items[out->count - 1] != '\\')
    {
        da_append(out, '/');
    }

    sb_append_sv(out, include_name);
    sb_append_null(out);
}

bool preprocess_file(const char *filepath, String_Builder *out, Consts *consts)
{
    String_Builder file = {0};

    if (!read_entire_file(filepath, &file))
    {
        printf("Failed to read file: %s\n", filepath);
        return false;
    }

    String_View program = sb_to_sv(file);
    Lines lines = {0};
    Includes includes = {0};
    ConstLineNums const_line_nums = {0};

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

        if (sv_eq_ignore_case(command, "include"))
        {
            if (rest.count == 0)
            {
                printf("include requires a file\n");
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }

            da_append(&includes, i);
        } else if (sv_eq_ignore_case(command, "const")) {
            if (rest.count == 0)
            {
                printf("const requires a name and value\n");
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }
            String_View name = sv_chop_by_delim(&rest, ' ');
            name = sv_trim(name);
            rest = sv_trim(rest);
            if (rest.count == 0)
            {
                printf("const requires a name and value\n");
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }
            NUM num;

            if (!parse_num(temp_sv_to_cstr(rest), &num))
            {
                printf("const value must be a number: %.*s\n", (int)rest.count, rest.data);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }

            if (!const_define(consts, name, num))
            {
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }
            da_append(&const_line_nums, i);
        }
    }

    for (size_t i = 0; i < lines.count; i++)
    {
        bool is_const = false;
        for (size_t j = 0; j < const_line_nums.count; j++) {
            if (const_line_nums.items[j] == i) {
                is_const = true;
                break;
            }
        }

        if (is_const) {
            continue;
        }

        bool is_include = false;

        for (size_t j = 0; j < includes.count; j++)
        {
            if (includes.items[j] == i)
            {
                is_include = true;
                break;
            }
        }

        if (is_include)
        {
            String_View line = lines.items[i];
            String_View rest = line;

            String_View command = sv_chop_by_delim(&rest, ' ');
            command = sv_trim(command);
            rest = sv_trim(rest);

            String_Builder include_path = {0};
            make_include_path(filepath, rest, &include_path);

            if (!preprocess_file(include_path.items, out, consts))
            {
                sb_free(include_path);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                return false;
            }

            sb_free(include_path);
        }
        else
        {
            sb_append_sv(out, lines.items[i]);
            da_append(out, '\n');
        }
    }

    sb_free(file);
    da_free(lines);
    da_free(includes);
    da_free(const_line_nums);

    return true;
}

int console(Vm *vm)
{
    Memory memory = {0};
    Consts consts = {0};
    for (;;)
    {
        char input[INPUT_BUFFER_SIZE];
        printf("pdvm> ");
        if (!fgets(input, sizeof(input), stdin))
            break;
        if (exec_line(vm, &memory, &consts, true, sv_from_cstr(input)) == 2)
            break;
    }

    consts_free(consts);
    da_free(memory);
    return 0;
}

int exec_file(Vm *vm, char *filepath)
{
    Consts consts = {0};
    String_Builder expanded = {0};

    if (!preprocess_file(filepath, &expanded, &consts))
    {
        printf("Failed to preprocess file\n");
        return 1;
    }

    int result = exec_program(vm, sb_to_sv(expanded), &consts);

    consts_free(consts);
    sb_free(expanded);
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