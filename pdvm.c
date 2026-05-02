#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define INPUT_BUFFER_SIZE 255

typedef enum
{
    VALUE_U8,
    VALUE_I32,
    VALUE_I64,
    VALUE_F32,
    VALUE_F64,
    VALUE_PTR,
    VALUE_STRUCT,
} Value_Type;

typedef enum
{
    FIELD_U8,
    FIELD_I32,
    FIELD_I64,
    FIELD_F32,
    FIELD_F64,
    FIELD_PTR,
    FIELD_STRUCT,
} Field_Type;

typedef struct Struct_Def Struct_Def;

typedef struct
{
    Field_Type type;

    String_View struct_name;

    size_t offset;
    size_t size;
    size_t align;
} Struct_Field;

struct Struct_Def
{
    String_View name;

    Struct_Field *fields;
    size_t fields_count;
    size_t fields_capacity;

    size_t size;
    size_t align;
};

typedef struct
{
    Struct_Def *items;
    size_t count;
    size_t capacity;
} Struct_Defs;

typedef struct
{
    Struct_Def *def;
    uint8_t *data;
    size_t size;
} Struct_Value;

typedef struct
{
    Value_Type type;

    union
    {
        uint8_t u8;
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        void *ptr;
        Struct_Value *st;
    } as;
} Value;

typedef struct
{
    Value *items;
    size_t capacity;
    size_t count;
} Vm;

bool sv_eq_ignore_case(String_View a, const char *b);
void struct_value_free(Struct_Value *st);
Struct_Value *struct_value_clone(const Struct_Value *src);

Value value_u8(uint8_t num)
{
    return (Value){
        .type = VALUE_U8,
        .as.u8 = num,
    };
}

Value value_i32(int32_t num)
{
    return (Value){
        .type = VALUE_I32,
        .as.i32 = num,
    };
}

Value value_i64(int64_t num)
{
    return (Value){
        .type = VALUE_I64,
        .as.i64 = num,
    };
}

Value value_f32(float num)
{
    return (Value){
        .type = VALUE_F32,
        .as.f32 = num,
    };
}

Value value_f64(double num)
{
    return (Value){
        .type = VALUE_F64,
        .as.f64 = num,
    };
}

Value value_ptr(void *ptr)
{
    return (Value){
        .type = VALUE_PTR,
        .as.ptr = ptr,
    };
}

Value value_struct(Struct_Value *st)
{
    return (Value){
        .type = VALUE_STRUCT,
        .as.st = st,
    };
}

size_t align_up_size(size_t value, size_t align)
{
    if (align == 0)
        return value;

    size_t remainder = value % align;
    if (remainder == 0)
        return value;
    return value + (align - remainder);
}

void value_free(Value *value)
{
    if (value == NULL)
        return;

    if (value->type == VALUE_STRUCT)
    {
        struct_value_free(value->as.st);
    }

    *value = value_i64(0);
}

Value value_clone(Value src)
{
    if (src.type == VALUE_STRUCT)
    {
        return value_struct(struct_value_clone(src.as.st));
    }

    return src;
}

const char *value_type_name(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
        return "u8";
    case VALUE_I32:
        return "i32";
    case VALUE_I64:
        return "i64";
    case VALUE_F32:
        return "f32";
    case VALUE_F64:
        return "f64";
    case VALUE_PTR:
        return "ptr";
    case VALUE_STRUCT:
        return "struct";
    default:
        return "unknown";
    }
}

const char *field_type_name(Field_Type type)
{
    switch (type)
    {
    case FIELD_U8:
        return "u8";
    case FIELD_I32:
        return "i32";
    case FIELD_I64:
        return "i64";
    case FIELD_F32:
        return "f32";
    case FIELD_F64:
        return "f64";
    case FIELD_PTR:
        return "ptr";
    case FIELD_STRUCT:
        return "struct";
    default:
        return "unknown";
    }
}

bool field_type_parse(String_View sv, Field_Type *out)
{
    if (sv_eq_ignore_case(sv, "u8"))
        *out = FIELD_U8;
    else if (sv_eq_ignore_case(sv, "i32"))
        *out = FIELD_I32;
    else if (sv_eq_ignore_case(sv, "i64"))
        *out = FIELD_I64;
    else if (sv_eq_ignore_case(sv, "f32"))
        *out = FIELD_F32;
    else if (sv_eq_ignore_case(sv, "f64"))
        *out = FIELD_F64;
    else if (sv_eq_ignore_case(sv, "ptr"))
        *out = FIELD_PTR;
    else
        return false;

    return true;
}

bool field_type_size_align(Field_Type type, size_t *size, size_t *align)
{
    switch (type)
    {
    case FIELD_U8:
        *size = sizeof(uint8_t);
        *align = _Alignof(uint8_t);
        return true;
    case FIELD_I32:
        *size = sizeof(int32_t);
        *align = _Alignof(int32_t);
        return true;
    case FIELD_I64:
        *size = sizeof(int64_t);
        *align = _Alignof(int64_t);
        return true;
    case FIELD_F32:
        *size = sizeof(float);
        *align = _Alignof(float);
        return true;
    case FIELD_F64:
        *size = sizeof(double);
        *align = _Alignof(double);
        return true;
    case FIELD_PTR:
        *size = sizeof(void *);
        *align = _Alignof(void *);
        return true;
    default:
        return false;
    }
}

Value_Type field_type_to_value_type(Field_Type type)
{
    switch (type)
    {
    case FIELD_U8:
        return VALUE_U8;
    case FIELD_I32:
        return VALUE_I32;
    case FIELD_I64:
        return VALUE_I64;
    case FIELD_F32:
        return VALUE_F32;
    case FIELD_F64:
        return VALUE_F64;
    case FIELD_PTR:
        return VALUE_PTR;
    case FIELD_STRUCT:
        return VALUE_STRUCT;
    default:
        return VALUE_I64;
    }
}

bool field_matches_value(Field_Type type, Value value)
{
    return field_type_to_value_type(type) == value.type;
}

bool parse_scalar_value_type(String_View sv, Value_Type *out)
{
    if (sv_eq_ignore_case(sv, "u8"))
        *out = VALUE_U8;
    else if (sv_eq_ignore_case(sv, "i32"))
        *out = VALUE_I32;
    else if (sv_eq_ignore_case(sv, "i64"))
        *out = VALUE_I64;
    else if (sv_eq_ignore_case(sv, "f32"))
        *out = VALUE_F32;
    else if (sv_eq_ignore_case(sv, "f64"))
        *out = VALUE_F64;
    else
        return false;

    return true;
}

bool value_type_is_integer(Value_Type type)
{
    return type == VALUE_U8 || type == VALUE_I32 || type == VALUE_I64;
}

bool value_type_is_float(Value_Type type)
{
    return type == VALUE_F32 || type == VALUE_F64;
}

int value_integer_rank(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
        return 0;
    case VALUE_I32:
        return 1;
    case VALUE_I64:
        return 2;
    default:
        return -1;
    }
}

int value_float_rank(Value_Type type)
{
    switch (type)
    {
    case VALUE_F32:
        return 0;
    case VALUE_F64:
        return 1;
    default:
        return -1;
    }
}

bool value_is_numeric(Value value)
{
    return value_type_is_integer(value.type) || value_type_is_float(value.type);
}

bool value_supports_text_output(Value value)
{
    return value_is_numeric(value);
}

bool value_supports_scalar_print(Value value)
{
    return value.type != VALUE_STRUCT && value.type != VALUE_PTR;
}

bool value_require_numeric(Value value, const char *op_name)
{
    if (value_is_numeric(value))
        return true;

    printf("%s only supports numeric values\n", op_name);
    return false;
}

bool value_require_text_output(Value value, const char *op_name)
{
    if (value_supports_text_output(value))
        return true;

    printf("%s does not support %s values\n", op_name, value_type_name(value.type));
    return false;
}

bool parse_i64(const char *input, int64_t *out)
{
    char *end = NULL;
    errno = 0;
    long long value = strtoll(input, &end, 10);

    if (input == end || errno == ERANGE)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
        return false;

    *out = (int64_t)value;
    return true;
}

bool parse_i32(const char *input, int32_t *out)
{
    int64_t value = 0;

    if (!parse_i64(input, &value))
        return false;

    if (value < INT32_MIN || value > INT32_MAX)
        return false;

    *out = (int32_t)value;
    return true;
}

bool parse_u8(const char *input, uint8_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(input, &end, 10);

    if (input == end || errno == ERANGE)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0' || value > UINT8_MAX)
        return false;

    *out = (uint8_t)value;
    return true;
}

bool parse_f32(const char *input, float *out)
{
    char *end = NULL;
    errno = 0;
    float value = strtof(input, &end);

    if (input == end || errno == ERANGE)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

bool parse_f64(const char *input, double *out)
{
    char *end = NULL;
    errno = 0;
    double value = strtod(input, &end);

    if (input == end || errno == ERANGE)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

bool parse_default_value(const char *input, Value *out)
{
    int64_t i64 = 0;

    if (parse_i64(input, &i64))
    {
        *out = value_i64(i64);
        return true;
    }

    double f64 = 0.0;

    if (parse_f64(input, &f64))
    {
        *out = value_f64(f64);
        return true;
    }

    return false;
}

bool parse_typed_value(Value_Type type, const char *input, Value *out)
{
    switch (type)
    {
    case VALUE_U8:
    {
        uint8_t value = 0;
        if (!parse_u8(input, &value))
            return false;
        *out = value_u8(value);
        return true;
    }
    case VALUE_I32:
    {
        int32_t value = 0;
        if (!parse_i32(input, &value))
            return false;
        *out = value_i32(value);
        return true;
    }
    case VALUE_I64:
    {
        int64_t value = 0;
        if (!parse_i64(input, &value))
            return false;
        *out = value_i64(value);
        return true;
    }
    case VALUE_F32:
    {
        float value = 0.0f;
        if (!parse_f32(input, &value))
            return false;
        *out = value_f32(value);
        return true;
    }
    case VALUE_F64:
    {
        double value = 0.0;
        if (!parse_f64(input, &value))
            return false;
        *out = value_f64(value);
        return true;
    }
    default:
        return false;
    }
}

bool value_try_as_i64(Value value, int64_t *out)
{
    switch (value.type)
    {
    case VALUE_U8:
        *out = value.as.u8;
        return true;
    case VALUE_I32:
        *out = value.as.i32;
        return true;
    case VALUE_I64:
        *out = value.as.i64;
        return true;
    default:
        return false;
    }
}

int64_t value_as_i64(Value value)
{
    int64_t result = 0;
    value_try_as_i64(value, &result);
    return result;
}

double value_as_f64(Value value)
{
    switch (value.type)
    {
    case VALUE_U8:
        return (double)value.as.u8;
    case VALUE_I32:
        return (double)value.as.i32;
    case VALUE_I64:
        return (double)value.as.i64;
    case VALUE_F32:
        return (double)value.as.f32;
    case VALUE_F64:
        return value.as.f64;
    default:
        return 0.0;
    }
}

bool value_is_zero(Value value)
{
    switch (value.type)
    {
    case VALUE_U8:
        return value.as.u8 == 0;
    case VALUE_I32:
        return value.as.i32 == 0;
    case VALUE_I64:
        return value.as.i64 == 0;
    case VALUE_F32:
        return value.as.f32 == 0.0f;
    case VALUE_F64:
        return value.as.f64 == 0.0;
    default:
        return false;
    }
}

typedef struct
{
    String_View *items;
    size_t count;
    size_t capacity;
} Lines;

typedef struct
{
    String_View name;
    size_t ip;
} Label;

typedef struct
{
    Label *items;
    size_t count;
    size_t capacity;
} Labels;

typedef struct
{
    unsigned char *items;
    size_t count;
    size_t capacity;
} Memory;

typedef struct
{
    size_t *items;
    size_t count;
    size_t capacity;
} CallStack;

typedef struct
{
    size_t *items;
    size_t count;
    size_t capacity;
} Includes;

typedef struct
{
    String_View name;
    Value value;
} Const;

typedef struct
{
    Const *items;
    size_t count;
    size_t capacity;
} Consts;

typedef struct
{
    size_t *items;
    size_t count;
    size_t capacity;
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

String_View sv_trim_line_end(String_View sv)
{
    while (sv.count > 0)
    {
        char c = sv.data[sv.count - 1];
        if (c != '\n' && c != '\r')
            break;
        sv.count--;
    }

    return sv;
}

bool struct_def_lookup(Struct_Defs *defs, String_View name, Struct_Def **out)
{
    for (size_t i = 0; i < defs->count; i++)
    {
        if (sv_eq(defs->items[i].name, name))
        {
            *out = &defs->items[i];
            return true;
        }
    }

    return false;
}

void struct_field_free(Struct_Field *field)
{
    if (field->type == FIELD_STRUCT && field->struct_name.count > 0)
    {
        free((void *)field->struct_name.data);
    }
}

void struct_def_free(Struct_Def *def)
{
    for (size_t i = 0; i < def->fields_count; i++)
    {
        struct_field_free(&def->fields[i]);
    }

    free((void *)def->name.data);
    free(def->fields);
}

void struct_defs_free(Struct_Defs defs)
{
    for (size_t i = 0; i < defs.count; i++)
    {
        struct_def_free(&defs.items[i]);
    }

    da_free(defs);
}

Struct_Def *struct_def_clone(const Struct_Def *src)
{
    Struct_Def *copy = malloc(sizeof(*copy));
    if (copy == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    *copy = (Struct_Def){0};
    copy->name = sv_dup_owned(src->name);
    copy->size = src->size;
    copy->align = src->align;
    copy->fields_count = src->fields_count;
    copy->fields_capacity = src->fields_count;

    if (src->fields_count > 0)
    {
        copy->fields = malloc(src->fields_count * sizeof(*copy->fields));
        if (copy->fields == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        for (size_t i = 0; i < src->fields_count; i++)
        {
            copy->fields[i] = src->fields[i];
            if (copy->fields[i].type == FIELD_STRUCT && copy->fields[i].struct_name.count > 0)
            {
                copy->fields[i].struct_name = sv_dup_owned(src->fields[i].struct_name);
            }
        }
    }

    return copy;
}

void struct_value_free(Struct_Value *st)
{
    if (st == NULL)
        return;

    free(st->data);
    if (st->def != NULL)
    {
        struct_def_free(st->def);
        free(st->def);
    }
    free(st);
}

Struct_Value *struct_value_clone(const Struct_Value *src)
{
    if (src == NULL)
        return NULL;

    Struct_Value *copy = malloc(sizeof(*copy));
    if (copy == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    copy->def = struct_def_clone(src->def);
    copy->size = src->size;
    copy->data = NULL;

    if (src->size > 0)
    {
        copy->data = malloc(src->size);
        if (copy->data == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        memcpy(copy->data, src->data, src->size);
    }

    return copy;
}

bool struct_def_define(Struct_Defs *defs, Struct_Def def)
{
    Struct_Def *existing = NULL;

    if (struct_def_lookup(defs, def.name, &existing))
    {
        printf("Struct already exists: %.*s\n", (int)def.name.count, def.name.data);
        return false;
    }

    da_append(defs, def);
    return true;
}

bool parse_struct_line(String_View input, Struct_Def *out)
{
    String_View name = sv_chop_by_delim(&input, ' ');
    name = sv_trim(name);
    input = sv_trim(input);

    if (name.count == 0 || input.count == 0)
        return false;

    Struct_Def def = {0};
    def.name = sv_dup_owned(name);

    while (input.count > 0)
    {
        String_View field_sv = sv_chop_by_delim(&input, ' ');
        field_sv = sv_trim(field_sv);
        input = sv_trim(input);

        if (field_sv.count == 0)
            continue;

        Field_Type type = FIELD_U8;
        if (!field_type_parse(field_sv, &type) || type == FIELD_STRUCT)
        {
            printf("Unsupported struct field type: %.*s\n", (int)field_sv.count, field_sv.data);
            struct_def_free(&def);
            return false;
        }

        size_t field_size = 0;
        size_t field_align = 0;
        if (!field_type_size_align(type, &field_size, &field_align))
        {
            struct_def_free(&def);
            return false;
        }

        def.size = align_up_size(def.size, field_align);

        Struct_Field field = {
            .type = type,
            .struct_name = {0},
            .offset = def.size,
            .size = field_size,
            .align = field_align,
        };

        if (def.fields_count >= def.fields_capacity)
        {
            size_t new_capacity = def.fields_capacity == 0 ? 16 : def.fields_capacity * 2;
            Struct_Field *new_fields = realloc(def.fields, new_capacity * sizeof(*new_fields));
            if (new_fields == NULL)
            {
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }

            def.fields = new_fields;
            def.fields_capacity = new_capacity;
        }

        def.fields[def.fields_count++] = field;
        if (field_align > def.align)
            def.align = field_align;
        def.size += field_size;
    }

    if (def.fields_count == 0)
    {
        struct_def_free(&def);
        return false;
    }

    def.size = align_up_size(def.size, def.align);
    *out = def;
    return true;
}

bool memory_ensure(Memory *memory, int64_t addr)
{
    if (addr < 0)
    {
        printf("Memory address out of range\n");
        return false;
    }

    size_t index = (size_t)addr;
    size_t old_count = memory->count;

    if (index > (SIZE_MAX / sizeof(Value)) - 1)
    {
        printf("Memory address out of range\n");
        return false;
    }

    size_t needed = (index + 1) * sizeof(Value);

    if (needed > memory->count)
    {
        da_resize(memory, needed);

        Value zero = value_i64(0);
        for (size_t offset = old_count; offset < needed; offset += sizeof(Value))
        {
            memcpy(memory->items + offset, &zero, sizeof(zero));
        }
    }

    return true;
}

Value memory_get(Memory *memory, size_t index)
{
    Value value = value_i64(0);
    memcpy(&value, memory->items + (index * sizeof(Value)), sizeof(value));
    return value_clone(value);
}

void memory_set(Memory *memory, size_t index, Value value)
{
    Value existing = value_i64(0);
    memcpy(&existing, memory->items + (index * sizeof(Value)), sizeof(existing));
    value_free(&existing);

    Value stored = value_clone(value);
    memcpy(memory->items + (index * sizeof(Value)), &stored, sizeof(stored));
}

void memory_free(Memory memory)
{
    for (size_t offset = 0; offset < memory.count; offset += sizeof(Value))
    {
        Value value = value_i64(0);
        memcpy(&value, memory.items + offset, sizeof(value));
        value_free(&value);
    }

    da_free(memory);
}

bool value_try_as_index(Value value, size_t *out, const char *name)
{
    int64_t raw = 0;

    if (!value_try_as_i64(value, &raw))
    {
        printf("%s must be an integer value\n", name);
        return false;
    }

    if (raw < 0)
    {
        printf("%s out of range\n", name);
        return false;
    }

    *out = (size_t)raw;
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

Value_Type value_binary_result_type(Value a, Value b)
{
    if (value_type_is_float(a.type) || value_type_is_float(b.type))
    {
        int rank_a = value_float_rank(a.type);
        int rank_b = value_float_rank(b.type);
        return rank_a > rank_b ? a.type : b.type;
    }

    int rank_a = value_integer_rank(a.type);
    int rank_b = value_integer_rank(b.type);
    return rank_a > rank_b ? a.type : b.type;
}

Value value_from_i64_for_type(Value_Type type, int64_t value)
{
    switch (type)
    {
    case VALUE_U8:
        return value_u8((uint8_t)value);
    case VALUE_I32:
        return value_i32((int32_t)value);
    case VALUE_I64:
        return value_i64(value);
    default:
        return value_i64(value);
    }
}

Value value_from_f64_for_type(Value_Type type, double value)
{
    switch (type)
    {
    case VALUE_F32:
        return value_f32((float)value);
    case VALUE_F64:
        return value_f64(value);
    default:
        return value_f64(value);
    }
}

bool value_has_float(Value a, Value b)
{
    return value_type_is_float(a.type) || value_type_is_float(b.type);
}

Value value_add(Value a, Value b)
{
    Value_Type result_type = value_binary_result_type(a, b);

    if (value_type_is_float(result_type))
    {
        return value_from_f64_for_type(result_type, value_as_f64(a) + value_as_f64(b));
    }

    return value_from_i64_for_type(result_type, value_as_i64(a) + value_as_i64(b));
}

Value value_sub(Value a, Value b)
{
    Value_Type result_type = value_binary_result_type(a, b);

    if (value_type_is_float(result_type))
    {
        return value_from_f64_for_type(result_type, value_as_f64(a) - value_as_f64(b));
    }

    return value_from_i64_for_type(result_type, value_as_i64(a) - value_as_i64(b));
}

Value value_mul(Value a, Value b)
{
    Value_Type result_type = value_binary_result_type(a, b);

    if (value_type_is_float(result_type))
    {
        return value_from_f64_for_type(result_type, value_as_f64(a) * value_as_f64(b));
    }

    return value_from_i64_for_type(result_type, value_as_i64(a) * value_as_i64(b));
}

Value value_div(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        return value_from_f64_for_type(result_type, value_as_f64(a) / value_as_f64(b));
    }

    Value_Type result_type = value_binary_result_type(a, b);
    return value_from_i64_for_type(result_type, value_as_i64(a) / value_as_i64(b));
}

Value value_mod(Value a, Value b)
{
    Value_Type result_type = value_binary_result_type(a, b);

    if (value_type_is_float(result_type))
    {
        double lhs = value_as_f64(a);
        double rhs = value_as_f64(b);
        double quotient = lhs / rhs;
        double truncated = (double)((int64_t)quotient);
        return value_from_f64_for_type(result_type, lhs - (truncated * rhs));
    }

    return value_from_i64_for_type(result_type, value_as_i64(a) % value_as_i64(b));
}

Value value_exp(Value base, Value exponent)
{
    Value_Type result_type = value_binary_result_type(base, exponent);

    if (value_type_is_float(result_type))
    {
        int64_t steps = (int64_t)value_as_f64(exponent);
        double result = 1.0;
        double factor = value_as_f64(base);

        for (int64_t i = 0; i < steps; i++)
        {
            result *= factor;
        }

        return value_from_f64_for_type(result_type, result);
    }

    int64_t steps = value_as_i64(exponent);
    int64_t result = 1;
    int64_t factor = value_as_i64(base);

    for (int64_t i = 0; i < steps; i++)
    {
        result *= factor;
    }

    return value_from_i64_for_type(result_type, result);
}

int value_compare(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        double lhs = value_as_f64(a);
        double rhs = value_as_f64(b);

        if (lhs < rhs)
            return -1;
        if (lhs > rhs)
            return 1;
        return 0;
    }

    int64_t lhs = value_as_i64(a);
    int64_t rhs = value_as_i64(b);

    if (lhs < rhs)
        return -1;
    if (lhs > rhs)
        return 1;
    return 0;
}

bool value_equal(Value a, Value b)
{
    return value_compare(a, b) == 0;
}

bool value_compare_checked(Value a, Value b, int *out, const char *op_name)
{
    if (!value_require_numeric(a, op_name) || !value_require_numeric(b, op_name))
        return false;

    *out = value_compare(a, b);
    return true;
}

bool value_convert_scalar(Value src, Value_Type dst_type, Value *out)
{
    if (!value_is_numeric(src))
        return false;

    switch (dst_type)
    {
    case VALUE_U8:
        if (value_type_is_float(src.type))
            *out = value_u8((uint8_t)value_as_f64(src));
        else
            *out = value_u8((uint8_t)value_as_i64(src));
        return true;
    case VALUE_I32:
        if (value_type_is_float(src.type))
            *out = value_i32((int32_t)value_as_f64(src));
        else
            *out = value_i32((int32_t)value_as_i64(src));
        return true;
    case VALUE_I64:
        if (value_type_is_float(src.type))
            *out = value_i64((int64_t)value_as_f64(src));
        else
            *out = value_i64((int64_t)value_as_i64(src));
        return true;
    case VALUE_F32:
        *out = value_f32((float)value_as_f64(src));
        return true;
    case VALUE_F64:
        *out = value_f64(value_as_f64(src));
        return true;
    default:
        return false;
    }
}

Value struct_field_value_from_bytes(Struct_Field field, const uint8_t *data)
{
    switch (field.type)
    {
    case FIELD_U8:
    {
        uint8_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_u8(value);
    }
    case FIELD_I32:
    {
        int32_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i32(value);
    }
    case FIELD_I64:
    {
        int64_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i64(value);
    }
    case FIELD_F32:
    {
        float value = 0.0f;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_f32(value);
    }
    case FIELD_F64:
    {
        double value = 0.0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_f64(value);
    }
    case FIELD_PTR:
    {
        void *value = NULL;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_ptr(value);
    }
    default:
        return value_i64(0);
    }
}

bool struct_field_store_bytes(Struct_Field field, uint8_t *data, Value value)
{
    if (!field_matches_value(field.type, value))
        return false;

    switch (field.type)
    {
    case FIELD_U8:
        memcpy(data + field.offset, &value.as.u8, sizeof(value.as.u8));
        return true;
    case FIELD_I32:
        memcpy(data + field.offset, &value.as.i32, sizeof(value.as.i32));
        return true;
    case FIELD_I64:
        memcpy(data + field.offset, &value.as.i64, sizeof(value.as.i64));
        return true;
    case FIELD_F32:
        memcpy(data + field.offset, &value.as.f32, sizeof(value.as.f32));
        return true;
    case FIELD_F64:
        memcpy(data + field.offset, &value.as.f64, sizeof(value.as.f64));
        return true;
    case FIELD_PTR:
        memcpy(data + field.offset, &value.as.ptr, sizeof(value.as.ptr));
        return true;
    default:
        return false;
    }
}

void value_print_float(double value, const char *fmt)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), fmt, value);

    if (strchr(buffer, '.') == NULL && strchr(buffer, 'e') == NULL && strchr(buffer, 'E') == NULL)
    {
        size_t len = strlen(buffer);
        if (len + 2 < sizeof(buffer))
        {
            buffer[len] = '.';
            buffer[len + 1] = '0';
            buffer[len + 2] = '\0';
        }
    }

    fputs(buffer, stdout);
}

void value_print(Value value)
{
    switch (value.type)
    {
    case VALUE_U8:
        printf("%u", value.as.u8);
        break;
    case VALUE_I32:
        printf("%d", value.as.i32);
        break;
    case VALUE_I64:
        printf("%lld", (long long)value.as.i64);
        break;
    case VALUE_F32:
        value_print_float((double)value.as.f32, "%.9g");
        break;
    case VALUE_F64:
        value_print_float(value.as.f64, "%.17g");
        break;
    case VALUE_PTR:
        if (value.as.ptr == NULL)
            printf("NULL");
        else
            printf("%p", value.as.ptr);
        break;
    default:
        printf("<%s>", value_type_name(value.type));
        break;
    }
}

void value_dump(Value value)
{
    if (value.type == VALUE_PTR)
    {
        if (value.as.ptr == NULL)
            printf("ptr(NULL)");
        else
            printf("ptr(%p)", value.as.ptr);
        return;
    }

    if (value.type == VALUE_STRUCT)
    {
        Struct_Value *st = value.as.st;
        printf("%.*s{", (int)st->def->name.count, st->def->name.data);
        for (size_t i = 0; i < st->def->fields_count; i++)
        {
            Value field_value = struct_field_value_from_bytes(st->def->fields[i], st->data);
            value_dump(field_value);
            if (i + 1 != st->def->fields_count)
            {
                printf(", ");
            }
        }
        printf("}");
        return;
    }

    printf("%s(", value_type_name(value.type));
    value_print(value);
    printf(")");
}

unsigned char value_to_char(Value value)
{
    if (value_type_is_float(value.type))
        return (unsigned char)value_as_f64(value);

    return (unsigned char)value_as_i64(value);
}

void vm_push(Vm *vm, Value value)
{
    da_append(vm, value);
}

Struct_Value *struct_value_new(Struct_Def *def)
{
    Struct_Value *st = malloc(sizeof(*st));
    if (st == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    st->def = struct_def_clone(def);
    st->size = def->size;
    st->data = calloc(def->size == 0 ? 1 : def->size, 1);
    if (st->data == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    return st;
}

void vm_pushs(Vm *vm, const char *str)
{
    size_t len = strlen(str);

    for (size_t i = 0; i < len; i++)
    {
        vm_push(vm, value_u8((uint8_t)str[i]));
    }

    vm_push(vm, value_i64((int64_t)len));
}

void vm_inputs(Vm *vm)
{
    char input[INPUT_BUFFER_SIZE];

    if (!fgets(input, sizeof(input), stdin))
    {
        printf("Failed to read input\n");
        return;
    }

    size_t len = strlen(input);
    while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r'))
    {
        input[--len] = '\0';
    }

    vm_pushs(vm, input);
}

Value vm_pop(Vm *vm)
{
    return da_pop(vm);
}

bool vm_add(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "add") || !value_require_numeric(rhs, "add"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_add(lhs, rhs));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_sub(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "sub") || !value_require_numeric(rhs, "sub"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_sub(lhs, rhs));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_mul(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "mul") || !value_require_numeric(rhs, "mul"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_mul(lhs, rhs));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_div(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "div") || !value_require_numeric(rhs, "div"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_div(lhs, rhs));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_mod(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "mod") || !value_require_numeric(rhs, "mod"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_mod(lhs, rhs));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_exp(Vm *vm)
{
    Value exponent = vm_pop(vm);
    Value base = vm_pop(vm);

    if (!value_require_numeric(base, "exp") || !value_require_numeric(exponent, "exp"))
    {
        value_free(&base);
        value_free(&exponent);
        return false;
    }

    vm_push(vm, value_exp(base, exponent));
    value_free(&base);
    value_free(&exponent);
    return true;
}

void vm_dup(Vm *vm)
{
    vm_push(vm, value_clone(da_last(vm)));
}

void vm_swap(Vm *vm)
{
    Value first = vm_pop(vm);
    Value second = vm_pop(vm);
    vm_push(vm, first);
    vm_push(vm, second);
}

bool vm_print(Vm *vm)
{
    Value value = da_last(vm);
    if (!value_supports_scalar_print(value))
    {
        printf("print does not support %s values\n", value_type_name(value.type));
        return false;
    }

    value_print(value);
    return true;
}

void vm_dump(Vm *vm)
{
    printf("[");
    for (size_t i = 0; i < vm->count; i++)
    {
        value_dump(vm->items[i]);
        if (i + 1 != vm->count)
        {
            printf(", ");
        }
    }
    printf("]");
}

void vm_clear(Vm *vm)
{
    for (size_t i = 0; i < vm->count; i++)
    {
        value_free(&vm->items[i]);
    }

    vm->count = 0;
}

bool vm_neg(Vm *vm)
{
    Value value = vm_pop(vm);

    if (!value_require_numeric(value, "neg"))
    {
        value_free(&value);
        return false;
    }

    if (value_type_is_float(value.type))
    {
        vm_push(vm, value_from_f64_for_type(value.type, -value_as_f64(value)));
        value_free(&value);
        return true;
    }

    vm_push(vm, value_from_i64_for_type(value.type, -value_as_i64(value)));
    value_free(&value);
    return true;
}

bool vm_eq(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "eq"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp == 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_neq(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "neq"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp != 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_lt(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "lt"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp < 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_gt(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "gt"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp > 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_lte(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "lte"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp <= 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_gte(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);
    int cmp = 0;

    if (!value_compare_checked(lhs, rhs, &cmp, "gte"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(cmp >= 0));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_and(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "and") || !value_require_numeric(rhs, "and"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(!value_is_zero(lhs) && !value_is_zero(rhs)));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_or(Vm *vm)
{
    Value rhs = vm_pop(vm);
    Value lhs = vm_pop(vm);

    if (!value_require_numeric(lhs, "or") || !value_require_numeric(rhs, "or"))
    {
        value_free(&lhs);
        value_free(&rhs);
        return false;
    }

    vm_push(vm, value_i64(!value_is_zero(lhs) || !value_is_zero(rhs)));
    value_free(&lhs);
    value_free(&rhs);
    return true;
}

bool vm_not(Vm *vm)
{
    Value value = vm_pop(vm);

    if (!value_require_numeric(value, "not"))
    {
        value_free(&value);
        return false;
    }

    vm_push(vm, value_i64(value_is_zero(value)));
    value_free(&value);
    return true;
}

void vm_over(Vm *vm)
{
    vm_push(vm, value_clone(vm->items[vm->count - 2]));
}

void vm_input(Vm *vm)
{
    char input[INPUT_BUFFER_SIZE];

    if (!fgets(input, sizeof(input), stdin))
    {
        printf("Failed to read input\n");
        return;
    }

    Value value = value_i64(0);

    if (!parse_default_value(input, &value))
    {
        printf("Input must be a number\n");
        return;
    }

    vm_push(vm, value);
}

void vm_rot(Vm *vm)
{
    Value c = vm_pop(vm);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);

    vm_push(vm, b);
    vm_push(vm, c);
    vm_push(vm, a);
}

bool vm_read(Vm *vm, Memory *memory)
{
    Value addr_value = vm_pop(vm);
    size_t addr = 0;

    if (!value_try_as_index(addr_value, &addr, "Memory address"))
    {
        value_free(&addr_value);
        return false;
    }

    if (!memory_ensure(memory, (int64_t)addr))
    {
        value_free(&addr_value);
        return false;
    }

    vm_push(vm, memory_get(memory, addr));
    value_free(&addr_value);
    return true;
}

bool vm_write(Vm *vm, Memory *memory)
{
    Value addr_value = vm_pop(vm);
    Value value = vm_pop(vm);
    size_t addr = 0;

    if (!value_try_as_index(addr_value, &addr, "Memory address"))
    {
        value_free(&addr_value);
        value_free(&value);
        return false;
    }

    if (!memory_ensure(memory, (int64_t)addr))
    {
        value_free(&addr_value);
        value_free(&value);
        return false;
    }

    memory_set(memory, addr, value);
    value_free(&addr_value);
    value_free(&value);
    return true;
}

bool vm_printc(Vm *vm)
{
    Value value = da_last(vm);
    if (!value_require_text_output(value, "printc"))
        return false;

    printf("%c", value_to_char(value));
    return true;
}

bool vm_prints(Vm *vm)
{
    Value len_value = vm_pop(vm);
    size_t len = 0;

    if (!value_try_as_index(len_value, &len, "String length") || len > vm->count)
    {
        printf("Invalid string length\n");
        value_free(&len_value);
        return false;
    }

    size_t start = vm->count - len;

    for (size_t i = start; i < vm->count; i++)
    {
        if (!value_require_text_output(vm->items[i], "prints"))
        {
            value_free(&len_value);
            return false;
        }
    }

    for (size_t i = start; i < vm->count; i++)
    {
        fputc(value_to_char(vm->items[i]), stdout);
    }

    value_free(&len_value);
    return true;
}

bool vm_emit(Vm *vm)
{
    Value value = vm_pop(vm);
    if (!value_supports_scalar_print(value))
    {
        printf("emit does not support %s values\n", value_type_name(value.type));
        value_free(&value);
        return false;
    }

    value_print(value);
    value_free(&value);
    return true;
}

bool vm_emitc(Vm *vm)
{
    Value value = vm_pop(vm);
    if (!value_require_text_output(value, "emitc"))
    {
        value_free(&value);
        return false;
    }

    printf("%c", value_to_char(value));
    value_free(&value);
    return true;
}

bool vm_emits(Vm *vm)
{
    Value len_value = vm_pop(vm);
    size_t len = 0;

    if (!value_try_as_index(len_value, &len, "String length") || len > vm->count)
    {
        printf("Invalid string length\n");
        value_free(&len_value);
        return false;
    }

    size_t start = vm->count - len;

    for (size_t i = start; i < vm->count; i++)
    {
        if (!value_require_text_output(vm->items[i], "emits"))
        {
            value_free(&len_value);
            return false;
        }
    }

    for (size_t i = start; i < vm->count; i++)
    {
        fputc(value_to_char(vm->items[i]), stdout);
        value_free(&vm->items[i]);
    }

    vm->count = start;
    value_free(&len_value);
    return true;
}

void vm_dup2(Vm *vm)
{
    Value top = vm->items[vm->count - 1];
    Value next = vm->items[vm->count - 2];
    vm_push(vm, value_clone(next));
    vm_push(vm, value_clone(top));
}

void vm_nip(Vm *vm)
{
    Value top = vm_pop(vm);
    Value dropped = vm_pop(vm);
    value_free(&dropped);
    vm_push(vm, top);
}

void vm_tuck(Vm *vm)
{
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);

    vm_push(vm, value_clone(b));
    vm_push(vm, a);
    vm_push(vm, b);
}

bool vm_pack(Vm *vm, Struct_Def *def)
{
    if (vm->count < def->fields_count)
    {
        printf("pack %.*s requires %zu values on the stack\n", (int)def->name.count, def->name.data, def->fields_count);
        return false;
    }

    size_t start = vm->count - def->fields_count;
    for (size_t i = 0; i < def->fields_count; i++)
    {
        if (!field_matches_value(def->fields[i].type, vm->items[start + i]))
        {
            printf("pack %.*s field %zu requires %s, got %s\n",
                   (int)def->name.count,
                   def->name.data,
                   i,
                   field_type_name(def->fields[i].type),
                   value_type_name(vm->items[start + i].type));
            return false;
        }
    }

    Struct_Value *st = struct_value_new(def);
    for (size_t i = 0; i < def->fields_count; i++)
    {
        (void)struct_field_store_bytes(def->fields[i], st->data, vm->items[start + i]);
    }

    for (size_t i = start; i < vm->count; i++)
    {
        value_free(&vm->items[i]);
    }
    vm->count = start;

    vm_push(vm, value_struct(st));
    return true;
}

bool vm_get_field(Vm *vm, Struct_Def *def, size_t index)
{
    Value value = vm_pop(vm);

    if (value.type != VALUE_STRUCT)
    {
        printf("get requires a struct value\n");
        value_free(&value);
        return false;
    }

    if (!sv_eq(value.as.st->def->name, def->name))
    {
        printf("get expected struct %.*s\n", (int)def->name.count, def->name.data);
        value_free(&value);
        return false;
    }

    if (index >= def->fields_count)
    {
        printf("get index out of range for %.*s\n", (int)def->name.count, def->name.data);
        value_free(&value);
        return false;
    }

    Value field_value = struct_field_value_from_bytes(def->fields[index], value.as.st->data);
    value_free(&value);
    vm_push(vm, field_value);
    return true;
}

bool vm_set_field(Vm *vm, Struct_Def *def, size_t index)
{
    Value field_value = vm_pop(vm);
    Value struct_value = vm_pop(vm);

    if (struct_value.type != VALUE_STRUCT)
    {
        printf("set requires a struct value\n");
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (!sv_eq(struct_value.as.st->def->name, def->name))
    {
        printf("set expected struct %.*s\n", (int)def->name.count, def->name.data);
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (index >= def->fields_count)
    {
        printf("set index out of range for %.*s\n", (int)def->name.count, def->name.data);
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (!field_matches_value(def->fields[index].type, field_value))
    {
        printf("set %.*s field %zu requires %s, got %s\n",
               (int)def->name.count,
               def->name.data,
               index,
               field_type_name(def->fields[index].type),
               value_type_name(field_value.type));
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    (void)struct_field_store_bytes(def->fields[index], struct_value.as.st->data, field_value);
    value_free(&field_value);
    vm_push(vm, struct_value);
    return true;
}

void vm_depth(Vm *vm)
{
    vm_push(vm, value_i64((int64_t)vm->count));
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


bool const_lookup(Consts *consts, String_View name, Value *value)
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

bool const_define(Consts *consts, String_View name, Value value)
{
    Value existing_value;

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
        value_free(&consts.items[i].value);
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

bool parse_push_value(Consts *consts, String_View input, Value *out)
{
    const char *value_str = temp_sv_to_cstr(input);

    if (parse_default_value(value_str, out))
        return true;

    if (const_lookup(consts, input, out))
        return true;

    return false;
}

bool parse_explicit_value(Consts *consts, Value_Type type, String_View input, Value *out)
{
    const char *value_str = temp_sv_to_cstr(input);

    if (parse_typed_value(type, value_str, out))
        return true;

    if (const_lookup(consts, input, out))
    {
        if (out->type != type)
        {
            printf("Constant %.*s is %s, not %s\n",
                   (int)input.count,
                   input.data,
                   value_type_name(out->type),
                   value_type_name(type));
            return false;
        }

        return true;
    }

    return false;
}

bool parse_const_line(String_View input, String_View *name, Value *value)
{
    String_View first = sv_chop_by_delim(&input, ' ');
    first = sv_trim(first);
    input = sv_trim(input);

    if (first.count == 0)
        return false;

    Value_Type type = VALUE_I64;
    bool has_explicit_type = true;

    if (sv_eq_ignore_case(first, "u8"))
        type = VALUE_U8;
    else if (sv_eq_ignore_case(first, "i32"))
        type = VALUE_I32;
    else if (sv_eq_ignore_case(first, "i64"))
        type = VALUE_I64;
    else if (sv_eq_ignore_case(first, "f32"))
        type = VALUE_F32;
    else if (sv_eq_ignore_case(first, "f64"))
        type = VALUE_F64;
    else
        has_explicit_type = false;

    if (!has_explicit_type)
    {
        *name = first;
        if (input.count == 0)
            return false;
        return parse_typed_value(VALUE_I64, temp_sv_to_cstr(input), value);
    }

    *name = sv_chop_by_delim(&input, ' ');
    *name = sv_trim(*name);
    input = sv_trim(input);

    if (name->count == 0 || input.count == 0)
        return false;

    return parse_typed_value(type, temp_sv_to_cstr(input), value);
}

bool parse_size_t_sv(String_View sv, size_t *out)
{
    int64_t value = 0;

    if (!parse_i64(temp_sv_to_cstr(sv), &value) || value < 0)
        return false;

    *out = (size_t)value;
    return true;
}

int exec_line(Vm *vm, Memory *memory, Consts *consts, Struct_Defs *struct_defs, bool console, String_View line)
{
    line = sv_trim_line_end(line);
    line = sv_trim_left(line);

    if (line.count == 0)
        return 1;

    if (line.data[0] == '#')
        return 1;

    String_View command = sv_chop_by_delim(&line, ' ');
    command = sv_trim(command);
    String_View raw_arg = sv_trim_left(line);
    line = sv_trim(raw_arg);

    if (sv_eq_ignore_case(command, "halt"))
    {
        return 2;
    }
    else if (sv_eq_ignore_case(command, "struct"))
    {
        Struct_Def def = {0};

        if (!parse_struct_line(line, &def))
        {
            printf("struct requires a name and one or more supported field types\n");
            return 1;
        }

        if (!struct_def_define(struct_defs, def))
        {
            struct_def_free(&def);
            return 1;
        }
    }
    else if (sv_eq_ignore_case(command, "push"))
    {
        if (line.count == 0)
        {
            printf("push requires a number or constant\n");
            return 1;
        }

        Value value = value_i64(0);

        if (parse_push_value(consts, line, &value))
        {
            vm_push(vm, value);
        }
        else
        {
            printf("Invalid number or unknown constant: %.*s\n", (int)line.count, line.data);
            return 1;
        }
    }
    else if (sv_eq_ignore_case(command, "u8") ||
             sv_eq_ignore_case(command, "i32") ||
             sv_eq_ignore_case(command, "i64") ||
             sv_eq_ignore_case(command, "f32") ||
             sv_eq_ignore_case(command, "f64"))
    {
        if (line.count == 0)
        {
            printf("%.*s requires a value\n", (int)command.count, command.data);
            return 1;
        }

        Value_Type type = VALUE_I64;

        if (sv_eq_ignore_case(command, "u8"))
            type = VALUE_U8;
        else if (sv_eq_ignore_case(command, "i32"))
            type = VALUE_I32;
        else if (sv_eq_ignore_case(command, "i64"))
            type = VALUE_I64;
        else if (sv_eq_ignore_case(command, "f32"))
            type = VALUE_F32;
        else if (sv_eq_ignore_case(command, "f64"))
            type = VALUE_F64;

        Value value = value_i64(0);

        if (!parse_explicit_value(consts, type, line, &value))
        {
            printf("Invalid %s value or constant: %.*s\n", value_type_name(type), (int)line.count, line.data);
            return 1;
        }

        vm_push(vm, value);
    }
    else if (sv_eq_ignore_case(command, "pushs"))
    {
        if (raw_arg.count == 0)
        {
            printf("pushs requires a string\n");
            return 1;
        }

        const char *str = temp_sv_to_cstr(raw_arg);
        vm_pushs(vm, str);
    }
    else if (sv_eq_ignore_case(command, "convert"))
    {
        Value_Type target_type = VALUE_I64;

        if (line.count == 0)
        {
            printf("convert requires a scalar target type\n");
            return 1;
        }

        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        if (!parse_scalar_value_type(line, &target_type))
        {
            printf("convert only supports scalar target types: u8, i32, i64, f32, f64\n");
            return 1;
        }

        Value src = vm_pop(vm);
        Value converted = value_i64(0);

        if (!value_convert_scalar(src, target_type, &converted))
        {
            printf("convert only supports numeric scalar source values\n");
            value_free(&src);
            return 1;
        }

        value_free(&src);
        vm_push(vm, converted);
    }
    else if (sv_eq_ignore_case(command, "pack"))
    {
        Struct_Def *def = NULL;

        if (line.count == 0)
        {
            printf("pack requires a struct name\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, line, &def))
        {
            printf("Unknown struct: %.*s\n", (int)line.count, line.data);
            return 1;
        }

        if (!vm_pack(vm, def))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "get"))
    {
        String_View name = sv_chop_by_delim(&line, ' ');
        name = sv_trim(name);
        line = sv_trim(line);

        Struct_Def *def = NULL;
        size_t index = 0;

        if (name.count == 0 || line.count == 0)
        {
            printf("get requires a struct name and index\n");
            return 1;
        }

        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, name, &def))
        {
            printf("Unknown struct: %.*s\n", (int)name.count, name.data);
            return 1;
        }

        if (!parse_size_t_sv(line, &index))
        {
            printf("get index must be a non-negative integer\n");
            return 1;
        }

        if (!vm_get_field(vm, def, index))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "set"))
    {
        String_View name = sv_chop_by_delim(&line, ' ');
        name = sv_trim(name);
        line = sv_trim(line);

        Struct_Def *def = NULL;
        size_t index = 0;

        if (name.count == 0 || line.count == 0)
        {
            printf("set requires a struct name and index\n");
            return 1;
        }

        if (vm->count < 2)
        {
            printf("There is less than 2 values on the stack\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, name, &def))
        {
            printf("Unknown struct: %.*s\n", (int)name.count, name.data);
            return 1;
        }

        if (!parse_size_t_sv(line, &index))
        {
            printf("set index must be a non-negative integer\n");
            return 1;
        }

        if (!vm_set_field(vm, def, index))
            return 1;
    }
    else if (console && sv_eq_ignore_case(command, "const"))
    {
        String_View name = {0};
        Value value = value_i64(0);

        if (!parse_const_line(line, &name, &value))
        {
            printf("const requires a type, name, and value\n");
            return 1;
        }

        if (!const_define(consts, name, value))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "pop") || sv_eq_ignore_case(command, "drop"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        Value dropped = vm_pop(vm);
        value_free(&dropped);
    }
    else if (sv_eq_ignore_case(command, "print"))
    {
        if (vm->count == 0)
        {
            printf("Stack is empty\n");
            return 1;
        }

        if (!vm_print(vm))
            return 1;
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

        if (!vm_add(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "sub"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_sub(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "mul"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_mul(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "div"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_div(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "mod"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_mod(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "exp"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_exp(vm))
            return 1;
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

        if (!vm_neg(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "eq"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_eq(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "neq"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_neq(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "lt"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_lt(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "gt"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_gt(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "lte"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_lte(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "gte"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_gte(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "and"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_and(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "or"))
    {
        if (vm->count < 2)
        {
            printf("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_or(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "not"))
    {
        if (vm->count == 0)
        {
            printf("There needs to be at least one number on the stack\n");
            return 1;
        }

        if (!vm_not(vm))
            return 1;
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
        if (console)
            printf("input> ");
        vm_input(vm);
    }
    else if (sv_eq_ignore_case(command, "inputs"))
    {
        if (console)
            printf("inputs> ");
        vm_inputs(vm);
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

        if (!vm_printc(vm))
            return 1;
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

        if (!vm_prints(vm))
            return 1;
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

        if (!vm_emit(vm))
            return 1;
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

        if (!vm_emitc(vm))
            return 1;
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

int exec_program(Vm *vm, String_View program, Consts *consts, Struct_Defs *struct_defs)
{
    Lines lines = {0};
    Labels labels = {0};
    Memory memory = {0};
    CallStack call_stack = {0};

    while (program.count > 0)
    {
        String_View line = sv_chop_by_delim(&program, '\n');

        line = sv_trim_line_end(line);
        line = sv_trim_left(line);

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
            Value value = vm_pop(vm);
            if (!value_require_numeric(value, "jz"))
            {
                value_free(&value);
                return 1;
            }

            bool non_zero = !value_is_zero(value);
            value_free(&value);
            if (non_zero)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jnz"))
        {
            Value value = vm_pop(vm);
            if (!value_require_numeric(value, "jnz"))
            {
                value_free(&value);
                return 1;
            }

            bool is_zero = value_is_zero(value);
            value_free(&value);
            if (is_zero)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jneg"))
        {
            Value value = vm_pop(vm);
            int cmp = 0;
            if (!value_compare_checked(value, value_i64(0), &cmp, "jneg"))
            {
                value_free(&value);
                return 1;
            }

            value_free(&value);
            if (cmp >= 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jpos"))
        {
            Value value = vm_pop(vm);
            int cmp = 0;
            if (!value_compare_checked(value, value_i64(0), &cmp, "jpos"))
            {
                value_free(&value);
                return 1;
            }

            value_free(&value);
            if (cmp <= 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jlez"))
        {
            Value value = vm_pop(vm);
            int cmp = 0;
            if (!value_compare_checked(value, value_i64(0), &cmp, "jlez"))
            {
                value_free(&value);
                return 1;
            }

            value_free(&value);
            if (cmp > 0)
            {
                ip++;
                continue;
            }

            if (!vm_jump(labels, rest, &ip))
                return 1;
        }
        else if (sv_eq_ignore_case(command, "jgez"))
        {
            Value value = vm_pop(vm);
            int cmp = 0;
            if (!value_compare_checked(value, value_i64(0), &cmp, "jgez"))
            {
                value_free(&value);
                return 1;
            }

            value_free(&value);
            if (cmp < 0)
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "je"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (cmp != 0)
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "jne"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (cmp == 0)
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "jl"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (!(cmp < 0))
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "jg"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (!(cmp > 0))
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "jle"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (!(cmp <= 0))
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

            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            int cmp = 0;

            if (!value_compare_checked(b, a, &cmp, "jge"))
            {
                value_free(&a);
                value_free(&b);
                return 1;
            }

            value_free(&a);
            value_free(&b);

            if (!(cmp >= 0))
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
            if (exec_line(vm, &memory, consts, struct_defs, false, line) == 2)
                break;

            ip++;
        }
    }

    memory_free(memory);
    da_free(call_stack);
    da_free(labels);
    da_free(lines);

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

bool preprocess_file(const char *filepath, String_Builder *out, Consts *consts, Struct_Defs *struct_defs)
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
    ConstLineNums struct_line_nums = {0};

    while (program.count > 0)
    {
        String_View line = sv_chop_by_delim(&program, '\n');

        line = sv_trim_line_end(line);
        line = sv_trim_left(line);

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
                da_free(struct_line_nums);
                return false;
            }

            da_append(&includes, i);
        }
        else if (sv_eq_ignore_case(command, "const"))
        {
            if (rest.count == 0)
            {
                printf("const requires a type, name, and value\n");
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
                return false;
            }

            String_View name = {0};
            Value value = value_i64(0);

            if (!parse_const_line(rest, &name, &value))
            {
                printf("const requires a valid typed value: %.*s\n", (int)rest.count, rest.data);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
                return false;
            }

            if (!const_define(consts, name, value))
            {
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
                return false;
            }
            da_append(&const_line_nums, i);
        }
        else if (sv_eq_ignore_case(command, "struct"))
        {
            Struct_Def def = {0};

            if (!parse_struct_line(rest, &def))
            {
                printf("struct requires a valid name and field list: %.*s\n", (int)rest.count, rest.data);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
                return false;
            }

            if (!struct_def_define(struct_defs, def))
            {
                struct_def_free(&def);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
                return false;
            }

            da_append(&struct_line_nums, i);
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

        bool is_struct = false;
        for (size_t j = 0; j < struct_line_nums.count; j++) {
            if (struct_line_nums.items[j] == i) {
                is_struct = true;
                break;
            }
        }

        if (is_struct) {
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

            if (!preprocess_file(include_path.items, out, consts, struct_defs))
            {
                sb_free(include_path);
                sb_free(file);
                da_free(lines);
                da_free(includes);
                da_free(const_line_nums);
                da_free(struct_line_nums);
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
    da_free(struct_line_nums);

    return true;
}

int console(Vm *vm)
{
    Memory memory = {0};
    Consts consts = {0};
    Struct_Defs struct_defs = {0};
    for (;;)
    {
        char input[INPUT_BUFFER_SIZE];
        printf("pdvm> ");
        if (!fgets(input, sizeof(input), stdin))
            break;
        if (exec_line(vm, &memory, &consts, &struct_defs, true, sv_from_cstr(input)) == 2)
            break;
    }

    consts_free(consts);
    struct_defs_free(struct_defs);
    memory_free(memory);
    return 0;
}

int exec_file(Vm *vm, char *filepath)
{
    Consts consts = {0};
    Struct_Defs struct_defs = {0};
    String_Builder expanded = {0};

    if (!preprocess_file(filepath, &expanded, &consts, &struct_defs))
    {
        printf("Failed to preprocess file\n");
        consts_free(consts);
        struct_defs_free(struct_defs);
        sb_free(expanded);
        return 1;
    }

    int result = exec_program(vm, sb_to_sv(expanded), &consts, &struct_defs);

    consts_free(consts);
    struct_defs_free(struct_defs);
    sb_free(expanded);
    return result;
}

int main(int argc, char *argv[])
{
    Vm vm = {0};
    int result = 0;

    if (argc == 1)
    {
        result = console(&vm);
    }
    else
    {
        char *file_path = argv[1];
        result = exec_file(&vm, file_path);
    }

    vm_clear(&vm);
    da_free(vm);
    return result;
}
