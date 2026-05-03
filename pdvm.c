#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifndef PDVM_HAS_LIBFFI
#if defined(__has_include)
#if __has_include(<ffi.h>)
#define PDVM_HAS_LIBFFI 1
#endif
#endif
#endif

#ifndef PDVM_HAS_LIBFFI
#define PDVM_HAS_LIBFFI 0
#endif

#if PDVM_HAS_LIBFFI
#include <ffi.h>
#else
typedef struct _ffi_type ffi_type;
#endif

#include <inttypes.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)

#define INPUT_BUFFER_SIZE 255

typedef enum
{
    VALUE_U8,
    VALUE_I8,
    VALUE_I16,
    VALUE_U16,
    VALUE_I32,
    VALUE_U32,
    VALUE_I64,
    VALUE_U64,
    VALUE_F32,
    VALUE_F64,
    VALUE_PTR,
    VALUE_STRUCT,
} Value_Type;

typedef enum
{
    FIELD_U8,
    FIELD_I8,
    FIELD_I16,
    FIELD_U16,
    FIELD_I32,
    FIELD_U32,
    FIELD_I64,
    FIELD_U64,
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
    Struct_Def *struct_def;
    bool owns_struct_def;

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
    String_View name;
    void *handle;
} Dl_Lib;

typedef struct
{
    Dl_Lib *items;
    size_t count;
    size_t capacity;
} Dl_Libs;

typedef struct
{
    String_View name;
    void *ptr;
    void *owner_handle;
} Dl_Sym;

typedef struct
{
    Dl_Sym *items;
    size_t count;
    size_t capacity;
} Dl_Syms;

typedef struct
{
    Struct_Def *def;
    uint8_t *data;
    size_t size;
} Struct_Value;

typedef enum
{
    DLCALL_TYPE_VOID,
    DLCALL_TYPE_SCALAR,
    DLCALL_TYPE_PTR,
    DLCALL_TYPE_STR,
    DLCALL_TYPE_STRUCT,
} Dlcall_Type_Kind;

typedef struct
{
    Dlcall_Type_Kind kind;
    Value_Type scalar_type;
    Struct_Def *struct_def;
    ffi_type *ffi_type_ptr;
    bool owns_ffi_type;
} Dlcall_Type;

typedef struct
{
    Dlcall_Type type;
    void *value_storage;
    void *owned_buffer;
} Dlcall_Arg;

typedef struct
{
    Value_Type type;

    union
    {
        uint8_t u8;
        int8_t i8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
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
    char **ffi_strings;
    size_t ffi_strings_count;
    size_t ffi_strings_capacity;
} Vm;

bool sv_eq_ignore_case(String_View a, const char *b);
void struct_value_free(Struct_Value *st);
Struct_Value *struct_value_clone(const Struct_Value *src);
Struct_Def *struct_def_clone(const Struct_Def *src);
void struct_def_free(Struct_Def *def);
void dlcall_arg_free(Dlcall_Arg *arg);
bool struct_def_lookup(Struct_Defs *defs, String_View name, Struct_Def **out);
void vm_push(Vm *vm, Value value);
void vm_pushs(Vm *vm, const char *str);
Struct_Value *struct_value_new(Struct_Def *def);
void vm_track_ffi_string(Vm *vm, char *buffer);
bool vm_error_output(Vm *vm);

Value value_u8(uint8_t num)
{
    return (Value){
        .type = VALUE_U8,
        .as.u8 = num,
    };
}

Value value_i8(int8_t num)
{
    return (Value){
        .type = VALUE_I8,
        .as.i8 = num,
    };
}

Value value_i16(int16_t num)
{
    return (Value){
        .type = VALUE_I16,
        .as.i16 = num,
    };
}

Value value_u16(uint16_t num)
{
    return (Value){
        .type = VALUE_U16,
        .as.u16 = num,
    };
}

Value value_i32(int32_t num)
{
    return (Value){
        .type = VALUE_I32,
        .as.i32 = num,
    };
}

Value value_u32(uint32_t num)
{
    return (Value){
        .type = VALUE_U32,
        .as.u32 = num,
    };
}

Value value_i64(int64_t num)
{
    return (Value){
        .type = VALUE_I64,
        .as.i64 = num,
    };
}

Value value_u64(uint64_t num)
{
    return (Value){
        .type = VALUE_U64,
        .as.u64 = num,
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
    case VALUE_I8:
        return "i8";
    case VALUE_I16:
        return "i16";
    case VALUE_U16:
        return "u16";
    case VALUE_I32:
        return "i32";
    case VALUE_U32:
        return "u32";
    case VALUE_I64:
        return "i64";
    case VALUE_U64:
        return "u64";
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
    case FIELD_I8:
        return "i8";
    case FIELD_I16:
        return "i16";
    case FIELD_U16:
        return "u16";
    case FIELD_I32:
        return "i32";
    case FIELD_U32:
        return "u32";
    case FIELD_I64:
        return "i64";
    case FIELD_U64:
        return "u64";
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
    else if (sv_eq_ignore_case(sv, "i8"))
        *out = FIELD_I8;
    else if (sv_eq_ignore_case(sv, "i16"))
        *out = FIELD_I16;
    else if (sv_eq_ignore_case(sv, "u16"))
        *out = FIELD_U16;
    else if (sv_eq_ignore_case(sv, "i32"))
        *out = FIELD_I32;
    else if (sv_eq_ignore_case(sv, "u32"))
        *out = FIELD_U32;
    else if (sv_eq_ignore_case(sv, "i64"))
        *out = FIELD_I64;
    else if (sv_eq_ignore_case(sv, "u64"))
        *out = FIELD_U64;
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
    case FIELD_I8:
        *size = sizeof(int8_t);
        *align = _Alignof(int8_t);
        return true;
    case FIELD_I16:
        *size = sizeof(int16_t);
        *align = _Alignof(int16_t);
        return true;
    case FIELD_U16:
        *size = sizeof(uint16_t);
        *align = _Alignof(uint16_t);
        return true;
    case FIELD_I32:
        *size = sizeof(int32_t);
        *align = _Alignof(int32_t);
        return true;
    case FIELD_U32:
        *size = sizeof(uint32_t);
        *align = _Alignof(uint32_t);
        return true;
    case FIELD_I64:
        *size = sizeof(int64_t);
        *align = _Alignof(int64_t);
        return true;
    case FIELD_U64:
        *size = sizeof(uint64_t);
        *align = _Alignof(uint64_t);
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

bool struct_field_size_align(Struct_Field field, size_t *size, size_t *align)
{
    if (field.type == FIELD_STRUCT)
    {
        if (field.struct_def == NULL)
            return false;

        *size = field.struct_def->size;
        *align = field.struct_def->align;
        return true;
    }

    return field_type_size_align(field.type, size, align);
}

Value_Type field_type_to_value_type(Field_Type type)
{
    switch (type)
    {
    case FIELD_U8:
        return VALUE_U8;
    case FIELD_I8:
        return VALUE_I8;
    case FIELD_I16:
        return VALUE_I16;
    case FIELD_U16:
        return VALUE_U16;
    case FIELD_I32:
        return VALUE_I32;
    case FIELD_U32:
        return VALUE_U32;
    case FIELD_I64:
        return VALUE_I64;
    case FIELD_U64:
        return VALUE_U64;
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

bool field_matches_value(Struct_Field field, Value value)
{
    if (field.type != FIELD_STRUCT)
        return field_type_to_value_type(field.type) == value.type;

    if (value.type != VALUE_STRUCT)
        return false;

    if (field.struct_def != NULL)
        return sv_eq(field.struct_def->name, value.as.st->def->name);

    if (field.struct_name.count > 0)
        return sv_eq(field.struct_name, value.as.st->def->name);

    return false;
}

void field_type_label(Struct_Field field, char *buffer, size_t buffer_size)
{
    if (field.type == FIELD_STRUCT && field.struct_name.count > 0)
    {
        snprintf(buffer, buffer_size, "%.*s", (int)field.struct_name.count, field.struct_name.data);
        return;
    }

    snprintf(buffer, buffer_size, "%s", field_type_name(field.type));
}

bool parse_scalar_value_type(String_View sv, Value_Type *out)
{
    if (sv_eq_ignore_case(sv, "u8"))
        *out = VALUE_U8;
    else if (sv_eq_ignore_case(sv, "i8"))
        *out = VALUE_I8;
    else if (sv_eq_ignore_case(sv, "i16"))
        *out = VALUE_I16;
    else if (sv_eq_ignore_case(sv, "u16"))
        *out = VALUE_U16;
    else if (sv_eq_ignore_case(sv, "i32"))
        *out = VALUE_I32;
    else if (sv_eq_ignore_case(sv, "u32"))
        *out = VALUE_U32;
    else if (sv_eq_ignore_case(sv, "i64"))
        *out = VALUE_I64;
    else if (sv_eq_ignore_case(sv, "u64"))
        *out = VALUE_U64;
    else if (sv_eq_ignore_case(sv, "f32"))
        *out = VALUE_F32;
    else if (sv_eq_ignore_case(sv, "f64"))
        *out = VALUE_F64;
    else
        return false;

    return true;
}

size_t value_type_storage_size(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
        return sizeof(uint8_t);
    case VALUE_I8:
        return sizeof(int8_t);
    case VALUE_I16:
        return sizeof(int16_t);
    case VALUE_U16:
        return sizeof(uint16_t);
    case VALUE_I32:
        return sizeof(int32_t);
    case VALUE_U32:
        return sizeof(uint32_t);
    case VALUE_I64:
        return sizeof(int64_t);
    case VALUE_U64:
        return sizeof(uint64_t);
    case VALUE_F32:
        return sizeof(float);
    case VALUE_F64:
        return sizeof(double);
    case VALUE_PTR:
        return sizeof(void *);
    default:
        return 0;
    }
}

const char *dlcall_type_kind_name(Dlcall_Type type)
{
    switch (type.kind)
    {
    case DLCALL_TYPE_VOID:
        return "void";
    case DLCALL_TYPE_SCALAR:
        return value_type_name(type.scalar_type);
    case DLCALL_TYPE_PTR:
        return "ptr";
    case DLCALL_TYPE_STR:
        return "str";
    case DLCALL_TYPE_STRUCT:
        return type.struct_def == NULL ? "struct" : temp_sv_to_cstr(type.struct_def->name);
    default:
        return "unknown";
    }
}

#if PDVM_HAS_LIBFFI
ffi_type *ffi_type_from_value_type(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
        return &ffi_type_uint8;
    case VALUE_I8:
        return &ffi_type_sint8;
    case VALUE_I16:
        return &ffi_type_sint16;
    case VALUE_U16:
        return &ffi_type_uint16;
    case VALUE_I32:
        return &ffi_type_sint32;
    case VALUE_U32:
        return &ffi_type_uint32;
    case VALUE_I64:
        return &ffi_type_sint64;
    case VALUE_U64:
        return &ffi_type_uint64;
    case VALUE_F32:
        return &ffi_type_float;
    case VALUE_F64:
        return &ffi_type_double;
    case VALUE_PTR:
        return &ffi_type_pointer;
    default:
        return NULL;
    }
}

void ffi_type_destroy_for_struct(ffi_type *type, Struct_Def *def);

ffi_type *ffi_type_build_for_struct(Struct_Def *def)
{
    ffi_type *type = calloc(1, sizeof(*type));
    ffi_type **elements = calloc(def->fields_count + 1, sizeof(*elements));

    if (type == NULL || elements == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (size_t i = 0; i < def->fields_count; i++)
    {
        if (def->fields[i].type == FIELD_STRUCT)
        {
            if (def->fields[i].struct_def == NULL)
            {
                free(elements);
                free(type);
                return NULL;
            }

            elements[i] = ffi_type_build_for_struct(def->fields[i].struct_def);
            if (elements[i] == NULL)
            {
                for (size_t j = 0; j < i; j++)
                {
                    if (def->fields[j].type == FIELD_STRUCT)
                    {
                        ffi_type_destroy_for_struct(elements[j], def->fields[j].struct_def);
                    }
                }
                free(elements);
                free(type);
                return NULL;
            }
            continue;
        }

        elements[i] = ffi_type_from_value_type(field_type_to_value_type(def->fields[i].type));
        if (elements[i] == NULL)
        {
            for (size_t j = 0; j < i; j++)
            {
                if (def->fields[j].type == FIELD_STRUCT)
                {
                    ffi_type_destroy_for_struct(elements[j], def->fields[j].struct_def);
                }
            }
            free(elements);
            free(type);
            return NULL;
        }
    }

    type->type = FFI_TYPE_STRUCT;
    type->elements = elements;
    return type;
}

void ffi_type_destroy_for_struct(ffi_type *type, Struct_Def *def)
{
    if (type == NULL)
        return;

    if (def != NULL)
    {
        for (size_t i = 0; i < def->fields_count; i++)
        {
            if (def->fields[i].type == FIELD_STRUCT)
            {
                ffi_type_destroy_for_struct(type->elements[i], def->fields[i].struct_def);
            }
        }
    }

    free(type->elements);
    free(type);
}
#else
ffi_type *ffi_type_from_value_type(Value_Type type)
{
    (void)type;
    return NULL;
}

ffi_type *ffi_type_build_for_struct(Struct_Def *def)
{
    (void)def;
    return NULL;
}

void ffi_type_destroy_for_struct(ffi_type *type, Struct_Def *def)
{
    (void)type;
    (void)def;
}
#endif

bool dlcall_type_parse(String_View token, Struct_Defs *struct_defs, bool allow_void, Dlcall_Type *out)
{
    *out = (Dlcall_Type){0};

    if (allow_void && sv_eq_ignore_case(token, "v"))
    {
        out->kind = DLCALL_TYPE_VOID;
        out->ffi_type_ptr = &ffi_type_void;
        return true;
    }

    if (sv_eq_ignore_case(token, "ptr"))
    {
        out->kind = DLCALL_TYPE_PTR;
        out->scalar_type = VALUE_PTR;
        out->ffi_type_ptr = ffi_type_from_value_type(VALUE_PTR);
        return true;
    }

    if (sv_eq_ignore_case(token, "str"))
    {
        out->kind = DLCALL_TYPE_STR;
        out->ffi_type_ptr = ffi_type_from_value_type(VALUE_PTR);
        return true;
    }

    Value_Type scalar_type = VALUE_I64;
    if (parse_scalar_value_type(token, &scalar_type))
    {
        out->kind = DLCALL_TYPE_SCALAR;
        out->scalar_type = scalar_type;
        out->ffi_type_ptr = ffi_type_from_value_type(scalar_type);
        return true;
    }

    Struct_Def *struct_def = NULL;
    if (struct_def_lookup(struct_defs, token, &struct_def))
    {
        out->kind = DLCALL_TYPE_STRUCT;
        out->struct_def = struct_def;
        out->ffi_type_ptr = ffi_type_build_for_struct(struct_def);
        out->owns_ffi_type = true;
        return out->ffi_type_ptr != NULL;
    }

    return false;
}

void dlcall_type_free(Dlcall_Type *type)
{
    if (type == NULL)
        return;

    if (type->owns_ffi_type)
    {
        ffi_type_destroy_for_struct(type->ffi_type_ptr, type->struct_def);
    }

    *type = (Dlcall_Type){0};
}

bool value_type_is_integer(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
    case VALUE_I8:
    case VALUE_I16:
    case VALUE_U16:
    case VALUE_I32:
    case VALUE_U32:
    case VALUE_I64:
    case VALUE_U64:
        return true;
    default:
        return false;
    }
}

bool value_type_is_float(Value_Type type)
{
    return type == VALUE_F32 || type == VALUE_F64;
}

bool value_type_is_signed_integer(Value_Type type)
{
    switch (type)
    {
    case VALUE_I8:
    case VALUE_I16:
    case VALUE_I32:
    case VALUE_I64:
        return true;
    default:
        return false;
    }
}

int value_integer_bits(Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
    case VALUE_I8:
        return 8;
    case VALUE_I16:
    case VALUE_U16:
        return 16;
    case VALUE_I32:
    case VALUE_U32:
        return 32;
    case VALUE_I64:
    case VALUE_U64:
        return 64;
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

typedef struct
{
    bool negative;
    unsigned __int128 magnitude;
} Exact_Int;

unsigned __int128 exact_uint_mask(int bits)
{
    return ((((unsigned __int128)1) << bits) - 1);
}

bool exact_int_from_value(Value value, Exact_Int *out)
{
    switch (value.type)
    {
    case VALUE_U8:
        *out = (Exact_Int){.negative = false, .magnitude = value.as.u8};
        return true;
    case VALUE_I8:
        *out = (Exact_Int){
            .negative = value.as.i8 < 0,
            .magnitude = (unsigned __int128)(value.as.i8 < 0 ? -(int64_t)value.as.i8 : (int64_t)value.as.i8),
        };
        return true;
    case VALUE_I16:
        *out = (Exact_Int){
            .negative = value.as.i16 < 0,
            .magnitude = (unsigned __int128)(value.as.i16 < 0 ? -(int64_t)value.as.i16 : (int64_t)value.as.i16),
        };
        return true;
    case VALUE_U16:
        *out = (Exact_Int){.negative = false, .magnitude = value.as.u16};
        return true;
    case VALUE_I32:
        *out = (Exact_Int){
            .negative = value.as.i32 < 0,
            .magnitude = (unsigned __int128)(value.as.i32 < 0 ? -(int64_t)value.as.i32 : (int64_t)value.as.i32),
        };
        return true;
    case VALUE_U32:
        *out = (Exact_Int){.negative = false, .magnitude = value.as.u32};
        return true;
    case VALUE_I64:
        if (value.as.i64 == INT64_MIN)
        {
            *out = (Exact_Int){
                .negative = true,
                .magnitude = ((unsigned __int128)1) << 63,
            };
        }
        else
        {
            *out = (Exact_Int){
                .negative = value.as.i64 < 0,
                .magnitude = (unsigned __int128)(value.as.i64 < 0 ? -value.as.i64 : value.as.i64),
            };
        }
        return true;
    case VALUE_U64:
        *out = (Exact_Int){.negative = false, .magnitude = value.as.u64};
        return true;
    default:
        return false;
    }
}

int exact_int_compare_abs(Exact_Int a, Exact_Int b)
{
    if (a.magnitude < b.magnitude)
        return -1;
    if (a.magnitude > b.magnitude)
        return 1;
    return 0;
}

int exact_int_compare(Exact_Int a, Exact_Int b)
{
    if (a.negative != b.negative)
        return a.negative ? -1 : 1;

    int cmp = exact_int_compare_abs(a, b);
    return a.negative ? -cmp : cmp;
}

Exact_Int exact_int_neg(Exact_Int value)
{
    if (value.magnitude != 0)
        value.negative = !value.negative;
    return value;
}

Exact_Int exact_int_add(Exact_Int a, Exact_Int b)
{
    if (a.negative == b.negative)
    {
        return (Exact_Int){
            .negative = a.negative,
            .magnitude = a.magnitude + b.magnitude,
        };
    }

    int cmp = exact_int_compare_abs(a, b);
    if (cmp == 0)
    {
        return (Exact_Int){0};
    }

    if (cmp > 0)
    {
        return (Exact_Int){
            .negative = a.negative,
            .magnitude = a.magnitude - b.magnitude,
        };
    }

    return (Exact_Int){
        .negative = b.negative,
        .magnitude = b.magnitude - a.magnitude,
    };
}

Exact_Int exact_int_sub(Exact_Int a, Exact_Int b)
{
    return exact_int_add(a, exact_int_neg(b));
}

Exact_Int exact_int_mul(Exact_Int a, Exact_Int b)
{
    unsigned __int128 magnitude = a.magnitude * b.magnitude;
    return (Exact_Int){
        .negative = magnitude != 0 && (a.negative != b.negative),
        .magnitude = magnitude,
    };
}

Exact_Int exact_int_div(Exact_Int a, Exact_Int b)
{
    unsigned __int128 magnitude = a.magnitude / b.magnitude;
    return (Exact_Int){
        .negative = magnitude != 0 && (a.negative != b.negative),
        .magnitude = magnitude,
    };
}

Exact_Int exact_int_mod(Exact_Int a, Exact_Int b)
{
    unsigned __int128 magnitude = a.magnitude % b.magnitude;
    return (Exact_Int){
        .negative = magnitude != 0 && a.negative,
        .magnitude = magnitude,
    };
}

bool exact_int_fits_in_type(Exact_Int value, Value_Type type)
{
    switch (type)
    {
    case VALUE_U8:
        return !value.negative && value.magnitude <= UINT8_MAX;
    case VALUE_I8:
        return value.negative ? value.magnitude <= 128 : value.magnitude <= INT8_MAX;
    case VALUE_I16:
        return value.negative ? value.magnitude <= 32768 : value.magnitude <= INT16_MAX;
    case VALUE_U16:
        return !value.negative && value.magnitude <= UINT16_MAX;
    case VALUE_I32:
        return value.negative ? value.magnitude <= 2147483648ULL : value.magnitude <= INT32_MAX;
    case VALUE_U32:
        return !value.negative && value.magnitude <= UINT32_MAX;
    case VALUE_I64:
        return value.negative ? value.magnitude <= ((((unsigned __int128)1) << 63)) : value.magnitude <= INT64_MAX;
    case VALUE_U64:
        return !value.negative && value.magnitude <= UINT64_MAX;
    default:
        return false;
    }
}

unsigned __int128 exact_int_to_twos_complement_bits(Exact_Int value, int bits)
{
    unsigned __int128 mask = exact_uint_mask(bits);
    unsigned __int128 magnitude = value.magnitude & mask;

    if (!value.negative)
        return magnitude;

    return ((((unsigned __int128)1) << bits) - magnitude) & mask;
}

int64_t exact_int_to_i64_lossless(Exact_Int value)
{
    if (!value.negative)
        return (int64_t)(uint64_t)value.magnitude;

    if (value.magnitude == ((((unsigned __int128)1) << 63)))
        return INT64_MIN;

    return -(int64_t)(uint64_t)value.magnitude;
}

bool exact_int_find_type(Exact_Int value, int min_bits, const Value_Type *candidates, size_t candidate_count, Value_Type *out)
{
    for (size_t i = 0; i < candidate_count; i++)
    {
        if (value_integer_bits(candidates[i]) < min_bits)
            continue;
        if (!exact_int_fits_in_type(value, candidates[i]))
            continue;

        *out = candidates[i];
        return true;
    }

    return false;
}

Value_Type value_integer_static_result_type(Value_Type a, Value_Type b)
{
    return value_integer_bits(a) >= value_integer_bits(b) ? a : b;
}

Value_Type value_integer_result_type(Value a, Value b, Exact_Int result)
{
    static const Value_Type signed_candidates[] = {
        VALUE_I8, VALUE_I16, VALUE_I32, VALUE_I64,
    };
    static const Value_Type unsigned_candidates[] = {
        VALUE_U8, VALUE_U16, VALUE_U32, VALUE_U64,
    };
    static const Value_Type mixed_candidates[] = {
        VALUE_I8, VALUE_U8, VALUE_I16, VALUE_U16, VALUE_I32, VALUE_U32, VALUE_I64, VALUE_U64,
    };

    int min_bits = value_integer_bits(a.type);
    if (value_integer_bits(b.type) > min_bits)
        min_bits = value_integer_bits(b.type);

    bool signed_a = value_type_is_signed_integer(a.type);
    bool signed_b = value_type_is_signed_integer(b.type);

    if (signed_a == signed_b)
    {
        Value_Type same_type = value_integer_static_result_type(a.type, b.type);
        if (exact_int_fits_in_type(result, same_type))
            return same_type;

        if (signed_a && exact_int_find_type(result, min_bits, signed_candidates, NOB_ARRAY_LEN(signed_candidates), &same_type))
            return same_type;
        if (!signed_a && exact_int_find_type(result, min_bits, unsigned_candidates, NOB_ARRAY_LEN(unsigned_candidates), &same_type))
            return same_type;
    }

    Value_Type result_type = result.negative ? VALUE_I64 : VALUE_U64;
    if (exact_int_find_type(result, min_bits, mixed_candidates, NOB_ARRAY_LEN(mixed_candidates), &result_type))
        return result_type;

    return result.negative ? VALUE_I64 : VALUE_U64;
}

Value value_from_exact_int_for_type(Value_Type type, Exact_Int value)
{
    switch (type)
    {
    case VALUE_U8:
        return value_u8((uint8_t)exact_int_to_twos_complement_bits(value, 8));
    case VALUE_I8:
    {
        uint8_t raw = (uint8_t)exact_int_to_twos_complement_bits(value, 8);
        int8_t num = 0;
        memcpy(&num, &raw, sizeof(num));
        return value_i8(num);
    }
    case VALUE_I16:
    {
        uint16_t raw = (uint16_t)exact_int_to_twos_complement_bits(value, 16);
        int16_t num = 0;
        memcpy(&num, &raw, sizeof(num));
        return value_i16(num);
    }
    case VALUE_U16:
        return value_u16((uint16_t)exact_int_to_twos_complement_bits(value, 16));
    case VALUE_I32:
    {
        uint32_t raw = (uint32_t)exact_int_to_twos_complement_bits(value, 32);
        int32_t num = 0;
        memcpy(&num, &raw, sizeof(num));
        return value_i32(num);
    }
    case VALUE_U32:
        return value_u32((uint32_t)exact_int_to_twos_complement_bits(value, 32));
    case VALUE_I64:
    {
        uint64_t raw = (uint64_t)exact_int_to_twos_complement_bits(value, 64);
        int64_t num = 0;
        memcpy(&num, &raw, sizeof(num));
        return value_i64(num);
    }
    case VALUE_U64:
        return value_u64((uint64_t)exact_int_to_twos_complement_bits(value, 64));
    default:
        return value_i64(exact_int_to_i64_lossless(value));
    }
}

bool value_require_numeric(Value value, const char *op_name)
{
    if (value_is_numeric(value))
        return true;

    ERR("%s only supports numeric values\n", op_name);
    return false;
}

bool value_require_text_output(Value value, const char *op_name)
{
    if (value_supports_text_output(value))
        return true;

    ERR("%s does not support %s values\n", op_name, value_type_name(value.type));
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

bool parse_i8(const char *input, int8_t *out)
{
    int64_t value = 0;

    if (!parse_i64(input, &value))
        return false;

    if (value < INT8_MIN || value > INT8_MAX)
        return false;

    *out = (int8_t)value;
    return true;
}

bool parse_i16(const char *input, int16_t *out)
{
    int64_t value = 0;

    if (!parse_i64(input, &value))
        return false;

    if (value < INT16_MIN || value > INT16_MAX)
        return false;

    *out = (int16_t)value;
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

bool parse_u64(const char *input, uint64_t *out)
{
    while (*input != '\0' && isspace((unsigned char)*input))
    {
        input++;
    }

    if (*input == '-')
        return false;

    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(input, &end, 10);

    if (input == end || errno == ERANGE)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
        return false;

    *out = (uint64_t)value;
    return true;
}

bool parse_u8(const char *input, uint8_t *out)
{
    uint64_t value = 0;

    if (!parse_u64(input, &value) || value > UINT8_MAX)
        return false;

    *out = (uint8_t)value;
    return true;
}

bool parse_u16(const char *input, uint16_t *out)
{
    uint64_t value = 0;

    if (!parse_u64(input, &value) || value > UINT16_MAX)
        return false;

    *out = (uint16_t)value;
    return true;
}

bool parse_u32(const char *input, uint32_t *out)
{
    uint64_t value = 0;

    if (!parse_u64(input, &value) || value > UINT32_MAX)
        return false;

    *out = (uint32_t)value;
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
    case VALUE_I8:
    {
        int8_t value = 0;
        if (!parse_i8(input, &value))
            return false;
        *out = value_i8(value);
        return true;
    }
    case VALUE_I16:
    {
        int16_t value = 0;
        if (!parse_i16(input, &value))
            return false;
        *out = value_i16(value);
        return true;
    }
    case VALUE_U16:
    {
        uint16_t value = 0;
        if (!parse_u16(input, &value))
            return false;
        *out = value_u16(value);
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
    case VALUE_U32:
    {
        uint32_t value = 0;
        if (!parse_u32(input, &value))
            return false;
        *out = value_u32(value);
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
    case VALUE_U64:
    {
        uint64_t value = 0;
        if (!parse_u64(input, &value))
            return false;
        *out = value_u64(value);
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
    case VALUE_I8:
        *out = value.as.i8;
        return true;
    case VALUE_I16:
        *out = value.as.i16;
        return true;
    case VALUE_U16:
        *out = value.as.u16;
        return true;
    case VALUE_I32:
        *out = value.as.i32;
        return true;
    case VALUE_U32:
        *out = value.as.u32;
        return true;
    case VALUE_I64:
        *out = value.as.i64;
        return true;
    case VALUE_U64:
        if (value.as.u64 > INT64_MAX)
            return false;
        *out = (int64_t)value.as.u64;
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
    case VALUE_I8:
        return (double)value.as.i8;
    case VALUE_I16:
        return (double)value.as.i16;
    case VALUE_U16:
        return (double)value.as.u16;
    case VALUE_I32:
        return (double)value.as.i32;
    case VALUE_U32:
        return (double)value.as.u32;
    case VALUE_I64:
        return (double)value.as.i64;
    case VALUE_U64:
        return (double)value.as.u64;
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
    case VALUE_I8:
        return value.as.i8 == 0;
    case VALUE_I16:
        return value.as.i16 == 0;
    case VALUE_U16:
        return value.as.u16 == 0;
    case VALUE_I32:
        return value.as.i32 == 0;
    case VALUE_U32:
        return value.as.u32 == 0;
    case VALUE_I64:
        return value.as.i64 == 0;
    case VALUE_U64:
        return value.as.u64 == 0;
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

typedef struct
{
    String_View name;
} Preproc_Def;

typedef struct
{
    Preproc_Def *items;
    size_t count;
    size_t capacity;
} Preproc_Defs;

typedef struct
{
    bool parent_active;
    bool active;
    bool branch_taken;
    bool saw_else;
} Preproc_Cond_Frame;

typedef struct
{
    Preproc_Cond_Frame *items;
    size_t count;
    size_t capacity;
} Preproc_Cond_Stack;

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

bool preproc_def_lookup(Preproc_Defs *defs, String_View name)
{
    for (size_t i = 0; i < defs->count; i++)
    {
        if (sv_eq(defs->items[i].name, name))
            return true;
    }

    return false;
}

bool preproc_defs_define(Preproc_Defs *defs, String_View name)
{
    if (preproc_def_lookup(defs, name))
        return true;

    Preproc_Def def = {
        .name = sv_dup_owned(name),
    };

    da_append(defs, def);
    return true;
}

void preproc_defs_undef(Preproc_Defs *defs, String_View name)
{
    for (size_t i = 0; i < defs->count; i++)
    {
        if (!sv_eq(defs->items[i].name, name))
            continue;

        free((void *)defs->items[i].name.data);
        for (size_t j = i + 1; j < defs->count; j++)
        {
            defs->items[j - 1] = defs->items[j];
        }
        defs->count--;
        return;
    }
}

void preproc_defs_free(Preproc_Defs defs)
{
    for (size_t i = 0; i < defs.count; i++)
    {
        free((void *)defs.items[i].name.data);
    }

    da_free(defs);
}

bool preproc_defs_add_host_builtins(Preproc_Defs *defs)
{
#ifdef _WIN32
    return preproc_defs_define(defs, sv_from_cstr("OS_WINDOWS"));
#elif defined(__APPLE__)
    return preproc_defs_define(defs, sv_from_cstr("OS_MACOS"));
#elif defined(__linux__)
    return preproc_defs_define(defs, sv_from_cstr("OS_LINUX"));
#else
    (void)defs;
    return true;
#endif
}

void struct_field_free(Struct_Field *field)
{
    if (field->type == FIELD_STRUCT && field->struct_name.count > 0)
    {
        free((void *)field->struct_name.data);
    }

    if (field->type == FIELD_STRUCT && field->owns_struct_def && field->struct_def != NULL)
    {
        struct_def_free(field->struct_def);
        free(field->struct_def);
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

bool dl_lib_lookup(Dl_Libs *libs, String_View name, Dl_Lib **out)
{
    for (size_t i = 0; i < libs->count; i++)
    {
        if (sv_eq(libs->items[i].name, name))
        {
            *out = &libs->items[i];
            return true;
        }
    }

    return false;
}

bool dl_sym_lookup(Dl_Syms *syms, String_View name, Dl_Sym **out)
{
    for (size_t i = 0; i < syms->count; i++)
    {
        if (sv_eq(syms->items[i].name, name))
        {
            *out = &syms->items[i];
            return true;
        }
    }

    return false;
}

bool dl_lib_define(Dl_Libs *libs, String_View name, void *handle)
{
    Dl_Lib *existing = NULL;

    if (dl_lib_lookup(libs, name, &existing))
    {
        ERR("Dynamic library already exists: %.*s\n", (int)name.count, name.data);
        return false;
    }

    Dl_Lib lib = {
        .name = sv_dup_owned(name),
        .handle = handle,
    };

    da_append(libs, lib);
    return true;
}

bool dl_sym_define(Dl_Syms *syms, String_View name, void *ptr, void *owner_handle)
{
    Dl_Sym *existing = NULL;

    if (dl_sym_lookup(syms, name, &existing))
    {
        ERR("Dynamic symbol already exists: %.*s\n", (int)name.count, name.data);
        return false;
    }

    Dl_Sym sym = {
        .name = sv_dup_owned(name),
        .ptr = ptr,
        .owner_handle = owner_handle,
    };

    da_append(syms, sym);
    return true;
}

void dl_syms_remove_by_owner(Dl_Syms *syms, void *owner_handle)
{
    size_t write_index = 0;

    for (size_t i = 0; i < syms->count; i++)
    {
        if (syms->items[i].owner_handle == owner_handle)
        {
            free((void *)syms->items[i].name.data);
            continue;
        }

        if (write_index != i)
        {
            syms->items[write_index] = syms->items[i];
        }
        write_index++;
    }

    syms->count = write_index;
}

#ifdef _WIN32
const char *vm_dl_last_error(void)
{
    static char buffer[512];
    DWORD error = GetLastError();

    DWORD written = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        0,
        buffer,
        (DWORD)sizeof(buffer),
        NULL);

    if (written == 0)
    {
        snprintf(buffer, sizeof(buffer), "Windows error %lu", (unsigned long)error);
    }

    return buffer;
}

void *vm_dlopen(const char *path)
{
    return (void *)LoadLibraryA(path);
}

void *vm_dlsym(void *handle, const char *symbol)
{
    return (void *)GetProcAddress((HMODULE)handle, symbol);
}

bool vm_dlclose(void *handle)
{
    return FreeLibrary((HMODULE)handle) != 0;
}
#else
const char *vm_dl_last_error(void)
{
    const char *error = dlerror();
    return error == NULL ? "Unknown dlerror" : error;
}

void *vm_dlopen(const char *path)
{
    return dlopen(path, RTLD_NOW);
}

void *vm_dlsym(void *handle, const char *symbol)
{
    dlerror();
    return dlsym(handle, symbol);
}

bool vm_dlclose(void *handle)
{
    return dlclose(handle) == 0;
}
#endif

void dl_libs_free(Dl_Libs libs)
{
    for (size_t i = 0; i < libs.count; i++)
    {
        if (libs.items[i].handle != NULL)
        {
            (void)vm_dlclose(libs.items[i].handle);
        }
        free((void *)libs.items[i].name.data);
    }

    da_free(libs);
}

void dl_syms_free(Dl_Syms syms)
{
    for (size_t i = 0; i < syms.count; i++)
    {
        free((void *)syms.items[i].name.data);
    }

    da_free(syms);
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
                if (src->fields[i].struct_def != NULL)
                {
                    copy->fields[i].struct_def = struct_def_clone(src->fields[i].struct_def);
                    copy->fields[i].owns_struct_def = true;
                }
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
        ERR("Struct already exists: %.*s\n", (int)def.name.count, def.name.data);
        return false;
    }

    da_append(defs, def);
    return true;
}

bool parse_struct_line(String_View input, Struct_Defs *defs, Struct_Def *out)
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
        Struct_Def *field_struct_def = NULL;
        String_View field_struct_name = {0};

        if (field_type_parse(field_sv, &type))
        {
            field_struct_name = (String_View){0};
        }
        else if (struct_def_lookup(defs, field_sv, &field_struct_def))
        {
            type = FIELD_STRUCT;
            field_struct_name = sv_dup_owned(field_sv);
        }
        else
        {
            ERR("Unsupported struct field type: %.*s\n", (int)field_sv.count, field_sv.data);
            struct_def_free(&def);
            return false;
        }

        size_t field_size = 0;
        size_t field_align = 0;
        Struct_Field field = {
            .type = type,
            .struct_name = field_struct_name,
            .struct_def = field_struct_def,
            .owns_struct_def = false,
            .offset = 0,
            .size = 0,
            .align = 0,
        };

        if (!struct_field_size_align(field, &field_size, &field_align))
        {
            if (field.type == FIELD_STRUCT && field.struct_name.count > 0)
                free((void *)field.struct_name.data);
            struct_def_free(&def);
            return false;
        }

        def.size = align_up_size(def.size, field_align);
        field.offset = def.size;
        field.size = field_size;
        field.align = field_align;

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

bool memory_ensure(Memory *memory, uint64_t addr)
{
    if (addr > SIZE_MAX)
    {
        ERR("Memory address out of range\n");
        return false;
    }

    size_t index = (size_t)addr;
    size_t old_count = memory->count;

    if (index > (SIZE_MAX / sizeof(Value)) - 1)
    {
        ERR("Memory address out of range\n");
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
        ERR("%s must be an integer value\n", name);
        return false;
    }

    if (raw < 0)
    {
        ERR("%s out of range\n", name);
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

    return value_integer_static_result_type(a.type, b.type);
}

Value value_from_i64_for_type(Value_Type type, int64_t value)
{
    switch (type)
    {
    case VALUE_U8:
        return value_u8((uint8_t)value);
    case VALUE_I8:
        return value_i8((int8_t)value);
    case VALUE_I16:
        return value_i16((int16_t)value);
    case VALUE_U16:
        return value_u16((uint16_t)value);
    case VALUE_I32:
        return value_i32((int32_t)value);
    case VALUE_U32:
        return value_u32((uint32_t)value);
    case VALUE_I64:
        return value_i64(value);
    case VALUE_U64:
        return value_u64((uint64_t)value);
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
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        return value_from_f64_for_type(result_type, value_as_f64(a) + value_as_f64(b));
    }

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    Exact_Int result = exact_int_add(lhs, rhs);
    Value_Type result_type = value_integer_result_type(a, b, result);
    return value_from_exact_int_for_type(result_type, result);
}

Value value_sub(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        return value_from_f64_for_type(result_type, value_as_f64(a) - value_as_f64(b));
    }

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    Exact_Int result = exact_int_sub(lhs, rhs);
    Value_Type result_type = value_integer_result_type(a, b, result);
    return value_from_exact_int_for_type(result_type, result);
}

Value value_mul(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        return value_from_f64_for_type(result_type, value_as_f64(a) * value_as_f64(b));
    }

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    Exact_Int result = exact_int_mul(lhs, rhs);
    Value_Type result_type = value_integer_result_type(a, b, result);
    return value_from_exact_int_for_type(result_type, result);
}

Value value_div(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        return value_from_f64_for_type(result_type, value_as_f64(a) / value_as_f64(b));
    }

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    Exact_Int result = exact_int_div(lhs, rhs);
    Value_Type result_type = value_integer_result_type(a, b, result);
    return value_from_exact_int_for_type(result_type, result);
}

Value value_mod(Value a, Value b)
{
    if (value_has_float(a, b))
    {
        Value_Type result_type = value_binary_result_type(a, b);
        double lhs = value_as_f64(a);
        double rhs = value_as_f64(b);
        double quotient = lhs / rhs;
        double truncated = (double)((int64_t)quotient);
        return value_from_f64_for_type(result_type, lhs - (truncated * rhs));
    }

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    Exact_Int result = exact_int_mod(lhs, rhs);
    Value_Type result_type = value_integer_result_type(a, b, result);
    return value_from_exact_int_for_type(result_type, result);
}

Value value_exp(Value base, Value exponent)
{
    if (value_has_float(base, exponent))
    {
        Value_Type result_type = value_binary_result_type(base, exponent);
        int64_t steps = (int64_t)value_as_f64(exponent);
        double result = 1.0;
        double factor = value_as_f64(base);

        for (int64_t i = 0; i < steps; i++)
        {
            result *= factor;
        }

        return value_from_f64_for_type(result_type, result);
    }

    Exact_Int factor = {0};
    Exact_Int steps = {0};
    Exact_Int result = {
        .negative = false,
        .magnitude = 1,
    };
    (void)exact_int_from_value(base, &factor);
    (void)exact_int_from_value(exponent, &steps);

    if (steps.negative)
        return value_from_exact_int_for_type(value_integer_result_type(base, exponent, result), result);

    for (uint64_t i = 0; i < (uint64_t)steps.magnitude; i++)
    {
        result = exact_int_mul(result, factor);
    }

    Value_Type result_type = value_integer_result_type(base, exponent, result);
    return value_from_exact_int_for_type(result_type, result);
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

    Exact_Int lhs = {0};
    Exact_Int rhs = {0};
    (void)exact_int_from_value(a, &lhs);
    (void)exact_int_from_value(b, &rhs);
    return exact_int_compare(lhs, rhs);
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
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_I8:
        if (value_type_is_float(src.type))
            *out = value_i8((int8_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_I16:
        if (value_type_is_float(src.type))
            *out = value_i16((int16_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_U16:
        if (value_type_is_float(src.type))
            *out = value_u16((uint16_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_I32:
        if (value_type_is_float(src.type))
            *out = value_i32((int32_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_U32:
        if (value_type_is_float(src.type))
            *out = value_u32((uint32_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_I64:
        if (value_type_is_float(src.type))
            *out = value_i64((int64_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
        return true;
    case VALUE_U64:
        if (value_type_is_float(src.type))
            *out = value_u64((uint64_t)value_as_f64(src));
        else
        {
            Exact_Int exact = {0};
            (void)exact_int_from_value(src, &exact);
            *out = value_from_exact_int_for_type(dst_type, exact);
        }
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
    case FIELD_I8:
    {
        int8_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i8(value);
    }
    case FIELD_I16:
    {
        int16_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i16(value);
    }
    case FIELD_U16:
    {
        uint16_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_u16(value);
    }
    case FIELD_I32:
    {
        int32_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i32(value);
    }
    case FIELD_U32:
    {
        uint32_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_u32(value);
    }
    case FIELD_I64:
    {
        int64_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_i64(value);
    }
    case FIELD_U64:
    {
        uint64_t value = 0;
        memcpy(&value, data + field.offset, sizeof(value));
        return value_u64(value);
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
    case FIELD_STRUCT:
    {
        if (field.struct_def == NULL)
            return value_i64(0);

        Struct_Value *st = struct_value_new(field.struct_def);
        memcpy(st->data, data + field.offset, field.size);
        return value_struct(st);
    }
    default:
        return value_i64(0);
    }
}

bool struct_field_store_bytes(Struct_Field field, uint8_t *data, Value value)
{
    if (!field_matches_value(field, value))
        return false;

    switch (field.type)
    {
    case FIELD_U8:
        memcpy(data + field.offset, &value.as.u8, sizeof(value.as.u8));
        return true;
    case FIELD_I8:
        memcpy(data + field.offset, &value.as.i8, sizeof(value.as.i8));
        return true;
    case FIELD_I16:
        memcpy(data + field.offset, &value.as.i16, sizeof(value.as.i16));
        return true;
    case FIELD_U16:
        memcpy(data + field.offset, &value.as.u16, sizeof(value.as.u16));
        return true;
    case FIELD_I32:
        memcpy(data + field.offset, &value.as.i32, sizeof(value.as.i32));
        return true;
    case FIELD_U32:
        memcpy(data + field.offset, &value.as.u32, sizeof(value.as.u32));
        return true;
    case FIELD_I64:
        memcpy(data + field.offset, &value.as.i64, sizeof(value.as.i64));
        return true;
    case FIELD_U64:
        memcpy(data + field.offset, &value.as.u64, sizeof(value.as.u64));
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
    case FIELD_STRUCT:
        memcpy(data + field.offset, value.as.st->data, field.size);
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
    case VALUE_I8:
        printf("%d", value.as.i8);
        break;
    case VALUE_I16:
        printf("%d", value.as.i16);
        break;
    case VALUE_U16:
        printf("%u", value.as.u16);
        break;
    case VALUE_I32:
        printf("%d", value.as.i32);
        break;
    case VALUE_U32:
        printf("%u", value.as.u32);
        break;
    case VALUE_I64:
        printf("%" PRId64, value.as.i64);
        break;
    case VALUE_U64:
        printf("%" PRIu64, value.as.u64);
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

    Exact_Int exact = {0};
    (void)exact_int_from_value(value, &exact);
    return (unsigned char)exact_int_to_twos_complement_bits(exact, 8);
}

bool value_is_text_byte(Value value)
{
    return value_is_numeric(value);
}

bool dlcall_arg_store_scalar(Value value, Value_Type type, void **out_storage)
{
    void *storage = calloc(1, value_type_storage_size(type));
    if (storage == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    switch (type)
    {
    case VALUE_U8:
        if (value.type != VALUE_U8)
            return free(storage), false;
        memcpy(storage, &value.as.u8, sizeof(value.as.u8));
        break;
    case VALUE_I8:
        if (value.type != VALUE_I8)
            return free(storage), false;
        memcpy(storage, &value.as.i8, sizeof(value.as.i8));
        break;
    case VALUE_I16:
        if (value.type != VALUE_I16)
            return free(storage), false;
        memcpy(storage, &value.as.i16, sizeof(value.as.i16));
        break;
    case VALUE_U16:
        if (value.type != VALUE_U16)
            return free(storage), false;
        memcpy(storage, &value.as.u16, sizeof(value.as.u16));
        break;
    case VALUE_I32:
        if (value.type != VALUE_I32)
            return free(storage), false;
        memcpy(storage, &value.as.i32, sizeof(value.as.i32));
        break;
    case VALUE_U32:
        if (value.type != VALUE_U32)
            return free(storage), false;
        memcpy(storage, &value.as.u32, sizeof(value.as.u32));
        break;
    case VALUE_I64:
        if (value.type != VALUE_I64)
            return free(storage), false;
        memcpy(storage, &value.as.i64, sizeof(value.as.i64));
        break;
    case VALUE_U64:
        if (value.type != VALUE_U64)
            return free(storage), false;
        memcpy(storage, &value.as.u64, sizeof(value.as.u64));
        break;
    case VALUE_F32:
        if (value.type != VALUE_F32)
            return free(storage), false;
        memcpy(storage, &value.as.f32, sizeof(value.as.f32));
        break;
    case VALUE_F64:
        if (value.type != VALUE_F64)
            return free(storage), false;
        memcpy(storage, &value.as.f64, sizeof(value.as.f64));
        break;
    default:
        free(storage);
        return false;
    }

    *out_storage = storage;
    return true;
}

void dlcall_arg_free(Dlcall_Arg *arg)
{
    if (arg == NULL)
        return;

    free(arg->value_storage);
    free(arg->owned_buffer);
    dlcall_type_free(&arg->type);
    *arg = (Dlcall_Arg){0};
}

void dlcall_args_free(Dlcall_Arg *args, size_t arg_count)
{
    if (args == NULL)
        return;

    for (size_t i = 0; i < arg_count; i++)
    {
        dlcall_arg_free(&args[i]);
    }

    free(args);
}

bool vm_collect_string_arg(Vm *vm, size_t *cursor, Dlcall_Arg *arg)
{
    if (*cursor == 0)
    {
        ERR("dlcall string argument is missing length\n");
        return false;
    }

    Value len_value = vm->items[*cursor - 1];
    if (len_value.type != VALUE_I64 || len_value.as.i64 < 0)
    {
        ERR("dlcall string argument requires i64 length on top of the stack\n");
        return false;
    }

    size_t len = (size_t)len_value.as.i64;
    if (len > (*cursor - 1))
    {
        ERR("dlcall string argument length is out of range\n");
        return false;
    }

    size_t start = *cursor - 1 - len;
    for (size_t i = start; i < *cursor - 1; i++)
    {
        if (!value_is_text_byte(vm->items[i]))
        {
            ERR("dlcall string argument contains non-text-compatible values\n");
            return false;
        }
    }

    char *buffer = malloc(len + 1);
    void *storage = calloc(1, sizeof(char *));
    if (buffer == NULL || storage == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (size_t i = 0; i < len; i++)
    {
        buffer[i] = (char)value_to_char(vm->items[start + i]);
    }
    buffer[len] = '\0';
    memcpy(storage, &buffer, sizeof(buffer));

    arg->value_storage = storage;
    arg->owned_buffer = NULL;
    vm_track_ffi_string(vm, buffer);
    *cursor = start;
    return true;
}

bool vm_collect_dlcall_arg(Vm *vm, size_t *cursor, Dlcall_Type parsed_type, Dlcall_Arg *arg)
{
    *arg = (Dlcall_Arg){
        .type = parsed_type,
    };

    switch (parsed_type.kind)
    {
    case DLCALL_TYPE_SCALAR:
        if (*cursor < 1)
        {
            ERR("dlcall requires more stack values for %s\n", value_type_name(parsed_type.scalar_type));
            return false;
        }
        if (!dlcall_arg_store_scalar(vm->items[*cursor - 1], parsed_type.scalar_type, &arg->value_storage))
        {
            ERR("dlcall expected %s, got %s\n",
                value_type_name(parsed_type.scalar_type),
                value_type_name(vm->items[*cursor - 1].type));
            return false;
        }
        (*cursor)--;
        return true;
    case DLCALL_TYPE_PTR:
        if (*cursor < 1)
        {
            ERR("dlcall requires more stack values for ptr\n");
            return false;
        }
        if (vm->items[*cursor - 1].type != VALUE_PTR)
        {
            ERR("dlcall expected ptr, got %s\n", value_type_name(vm->items[*cursor - 1].type));
            return false;
        }
        arg->value_storage = calloc(1, sizeof(void *));
        if (arg->value_storage == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }
        memcpy(arg->value_storage, &vm->items[*cursor - 1].as.ptr, sizeof(void *));
        (*cursor)--;
        return true;
    case DLCALL_TYPE_STR:
        return vm_collect_string_arg(vm, cursor, arg);
    case DLCALL_TYPE_STRUCT:
        if (*cursor < 1)
        {
            ERR("dlcall requires more stack values for struct %.*s\n",
                (int)parsed_type.struct_def->name.count,
                parsed_type.struct_def->name.data);
            return false;
        }
        if (vm->items[*cursor - 1].type != VALUE_STRUCT)
        {
            ERR("dlcall expected struct %.*s, got %s\n",
                (int)parsed_type.struct_def->name.count,
                parsed_type.struct_def->name.data,
                value_type_name(vm->items[*cursor - 1].type));
            return false;
        }
        if (!sv_eq(vm->items[*cursor - 1].as.st->def->name, parsed_type.struct_def->name))
        {
            ERR("dlcall expected struct %.*s\n",
                (int)parsed_type.struct_def->name.count,
                parsed_type.struct_def->name.data);
            return false;
        }
        arg->value_storage = malloc(parsed_type.struct_def->size == 0 ? 1 : parsed_type.struct_def->size);
        if (arg->value_storage == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }
        memcpy(arg->value_storage, vm->items[*cursor - 1].as.st->data, parsed_type.struct_def->size);
        (*cursor)--;
        return true;
    default:
        ERR("Unsupported dlcall argument type\n");
        return false;
    }
}

void vm_discard_stack_range(Vm *vm, size_t start)
{
    for (size_t i = start; i < vm->count; i++)
    {
        value_free(&vm->items[i]);
    }

    vm->count = start;
}

bool vm_push_dlcall_return(Vm *vm, Dlcall_Type ret_type, void *storage)
{
    switch (ret_type.kind)
    {
    case DLCALL_TYPE_VOID:
        return true;
    case DLCALL_TYPE_SCALAR:
        switch (ret_type.scalar_type)
        {
        case VALUE_U8:
            vm_push(vm, value_u8(*(uint8_t *)storage));
            return true;
        case VALUE_I8:
            vm_push(vm, value_i8(*(int8_t *)storage));
            return true;
        case VALUE_I16:
            vm_push(vm, value_i16(*(int16_t *)storage));
            return true;
        case VALUE_U16:
            vm_push(vm, value_u16(*(uint16_t *)storage));
            return true;
        case VALUE_I32:
            vm_push(vm, value_i32(*(int32_t *)storage));
            return true;
        case VALUE_U32:
            vm_push(vm, value_u32(*(uint32_t *)storage));
            return true;
        case VALUE_I64:
            vm_push(vm, value_i64(*(int64_t *)storage));
            return true;
        case VALUE_U64:
            vm_push(vm, value_u64(*(uint64_t *)storage));
            return true;
        case VALUE_F32:
            vm_push(vm, value_f32(*(float *)storage));
            return true;
        case VALUE_F64:
            vm_push(vm, value_f64(*(double *)storage));
            return true;
        default:
            return false;
        }
    case DLCALL_TYPE_PTR:
        vm_push(vm, value_ptr(*(void **)storage));
        return true;
    case DLCALL_TYPE_STR:
    {
        char *str = *(char **)storage;
        if (str == NULL)
        {
            vm_push(vm, value_i64(0));
        }
        else
        {
            vm_pushs(vm, str);
        }
        return true;
    }
    case DLCALL_TYPE_STRUCT:
    {
        Struct_Value *st = struct_value_new(ret_type.struct_def);
        memcpy(st->data, storage, ret_type.struct_def->size);
        vm_push(vm, value_struct(st));
        return true;
    }
    default:
        return false;
    }
}

bool vm_dlcall(Vm *vm, Struct_Defs *struct_defs, void *symbol_ptr, String_View symbol_name, Dlcall_Type ret_type, String_View *arg_tokens, size_t arg_count)
{
    (void)struct_defs;
#if !PDVM_HAS_LIBFFI
    (void)vm;
    (void)symbol_ptr;
    (void)symbol_name;
    (void)ret_type;
    (void)arg_tokens;
    (void)arg_count;
    ERR("dlcall requires pdvm to be built with libffi support\n");
    return false;
#else
    Dlcall_Arg *args = calloc(arg_count == 0 ? 1 : arg_count, sizeof(*args));
    ffi_type **arg_ffi_types = calloc(arg_count == 0 ? 1 : arg_count, sizeof(*arg_ffi_types));
    void **arg_values = calloc(arg_count == 0 ? 1 : arg_count, sizeof(*arg_values));
    void *ret_storage = NULL;
    size_t cursor = vm->count;
    bool result = false;

    if (args == NULL || arg_ffi_types == NULL || arg_values == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (size_t i = arg_count; i-- > 0;)
    {
        Dlcall_Type arg_type = {0};
        if (!dlcall_type_parse(arg_tokens[i], struct_defs, false, &arg_type))
        {
            ERR("Unknown dlcall argument type: %.*s\n", (int)arg_tokens[i].count, arg_tokens[i].data);
            goto defer;
        }

        if (!vm_collect_dlcall_arg(vm, &cursor, arg_type, &args[i]))
        {
            dlcall_type_free(&arg_type);
            goto defer;
        }

        arg_ffi_types[i] = args[i].type.ffi_type_ptr;
        arg_values[i] = args[i].value_storage;
    }

    ffi_cif cif = {0};
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)arg_count, ret_type.ffi_type_ptr, arg_ffi_types) != FFI_OK)
    {
        ERR("ffi_prep_cif failed for dlcall %.*s\n", (int)symbol_name.count, symbol_name.data);
        goto defer;
    }

    switch (ret_type.kind)
    {
    case DLCALL_TYPE_VOID:
        ret_storage = NULL;
        break;
    case DLCALL_TYPE_SCALAR:
        ret_storage = calloc(1, value_type_storage_size(ret_type.scalar_type));
        break;
    case DLCALL_TYPE_PTR:
    case DLCALL_TYPE_STR:
        ret_storage = calloc(1, sizeof(void *));
        break;
    case DLCALL_TYPE_STRUCT:
        ret_storage = calloc(1, ret_type.struct_def->size == 0 ? 1 : ret_type.struct_def->size);
        break;
    default:
        break;
    }

    if (ret_type.kind != DLCALL_TYPE_VOID && ret_storage == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    ffi_call(&cif, FFI_FN(symbol_ptr), ret_storage, arg_values);
    vm_discard_stack_range(vm, cursor);
    result = vm_push_dlcall_return(vm, ret_type, ret_storage);

defer:
    free(ret_storage);
    dlcall_args_free(args, arg_count);
    free(arg_ffi_types);
    free(arg_values);
    return result;
#endif
}

void vm_push(Vm *vm, Value value)
{
    da_append(vm, value);
}

void vm_track_ffi_string(Vm *vm, char *buffer)
{
    if (vm->ffi_strings_count >= vm->ffi_strings_capacity)
    {
        size_t new_capacity = vm->ffi_strings_capacity == 0 ? 16 : vm->ffi_strings_capacity * 2;
        char **new_items = realloc(vm->ffi_strings, new_capacity * sizeof(*new_items));
        if (new_items == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }
        vm->ffi_strings = new_items;
        vm->ffi_strings_capacity = new_capacity;
    }

    vm->ffi_strings[vm->ffi_strings_count++] = buffer;
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
        ERR("Failed to read input\n");
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
        ERR("print does not support %s values\n", value_type_name(value.type));
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

    for (size_t i = 0; i < vm->ffi_strings_count; i++)
    {
        free(vm->ffi_strings[i]);
    }
    free(vm->ffi_strings);
    vm->ffi_strings = NULL;
    vm->ffi_strings_count = 0;
    vm->ffi_strings_capacity = 0;
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

    Exact_Int exact = {0};
    (void)exact_int_from_value(value, &exact);
    Exact_Int result = exact_int_neg(exact);
    Value_Type result_type = value_integer_result_type(value, value, result);
    vm_push(vm, value_from_exact_int_for_type(result_type, result));
    value_free(&value);
    return true;
}

bool value_try_as_memory_address(Value value, uint64_t *out)
{
    if (value.type != VALUE_U64)
    {
        ERR("Memory address must be u64\n");
        return false;
    }

    *out = value.as.u64;
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
        ERR("Failed to read input\n");
        return;
    }

    Value value = value_i64(0);

    if (!parse_default_value(input, &value))
    {
        ERR("Input must be a number\n");
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
    uint64_t addr = 0;

    if (!value_try_as_memory_address(addr_value, &addr))
    {
        value_free(&addr_value);
        return false;
    }

    if (!memory_ensure(memory, addr))
    {
        value_free(&addr_value);
        return false;
    }

    vm_push(vm, memory_get(memory, (size_t)addr));
    value_free(&addr_value);
    return true;
}

bool vm_write(Vm *vm, Memory *memory)
{
    Value addr_value = vm_pop(vm);
    Value value = vm_pop(vm);
    uint64_t addr = 0;

    if (!value_try_as_memory_address(addr_value, &addr))
    {
        value_free(&addr_value);
        value_free(&value);
        return false;
    }

    if (!memory_ensure(memory, addr))
    {
        value_free(&addr_value);
        value_free(&value);
        return false;
    }

    memory_set(memory, (size_t)addr, value);
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
        ERR("Invalid string length\n");
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
        ERR("emit does not support %s values\n", value_type_name(value.type));
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
        ERR("Invalid string length\n");
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

bool vm_error_output(Vm *vm)
{
    Value len_value = vm_pop(vm);
    size_t len = 0;

    if (!value_try_as_index(len_value, &len, "String length") || len > vm->count)
    {
        ERR("Invalid string length\n");
        value_free(&len_value);
        return false;
    }

    size_t start = vm->count - len;

    for (size_t i = start; i < vm->count; i++)
    {
        if (!value_require_text_output(vm->items[i], "error"))
        {
            value_free(&len_value);
            return false;
        }
    }

    for (size_t i = start; i < vm->count; i++)
    {
        fputc(value_to_char(vm->items[i]), stderr);
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
        ERR("pack %.*s requires %zu values on the stack\n", (int)def->name.count, def->name.data, def->fields_count);
        return false;
    }

    size_t start = vm->count - def->fields_count;
    for (size_t i = 0; i < def->fields_count; i++)
    {
        if (!field_matches_value(def->fields[i], vm->items[start + i]))
        {
            char expected[128];
            field_type_label(def->fields[i], expected, sizeof(expected));
            ERR("pack %.*s field %zu requires %s, got %s\n",
                (int)def->name.count,
                def->name.data,
                i,
                expected,
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
        ERR("get requires a struct value\n");
        value_free(&value);
        return false;
    }

    if (!sv_eq(value.as.st->def->name, def->name))
    {
        ERR("get expected struct %.*s\n", (int)def->name.count, def->name.data);
        value_free(&value);
        return false;
    }

    if (index >= def->fields_count)
    {
        ERR("get index out of range for %.*s\n", (int)def->name.count, def->name.data);
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
        ERR("set requires a struct value\n");
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (!sv_eq(struct_value.as.st->def->name, def->name))
    {
        ERR("set expected struct %.*s\n", (int)def->name.count, def->name.data);
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (index >= def->fields_count)
    {
        ERR("set index out of range for %.*s\n", (int)def->name.count, def->name.data);
        value_free(&field_value);
        value_free(&struct_value);
        return false;
    }

    if (!field_matches_value(def->fields[index], field_value))
    {
        char expected[128];
        field_type_label(def->fields[index], expected, sizeof(expected));
        ERR("set %.*s field %zu requires %s, got %s\n",
            (int)def->name.count,
            def->name.data,
            index,
            expected,
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
        ERR("Unknown label: %.*s\n", (int)label_name.count, label_name.data);
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
        ERR("Constant already exists: %.*s\n", (int)name.count, name.data);
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
            ERR("Constant %.*s is %s, not %s\n",
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

    if (!parse_scalar_value_type(first, &type))
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

int exec_line(Vm *vm, Memory *memory, Consts *consts, Struct_Defs *struct_defs, Dl_Libs *dl_libs, Dl_Syms *dl_syms, bool console, String_View line)
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

        if (!parse_struct_line(line, struct_defs, &def))
        {
            ERR("struct requires a name and one or more supported field types\n");
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
            ERR("push requires a number or constant\n");
            return 1;
        }

        Value value = value_i64(0);

        if (parse_push_value(consts, line, &value))
        {
            vm_push(vm, value);
        }
        else
        {
            ERR("Invalid number or unknown constant: %.*s\n", (int)line.count, line.data);
            return 1;
        }
    }
    else if (parse_scalar_value_type(command, &(Value_Type){0}))
    {
        if (line.count == 0)
        {
            ERR("%.*s requires a value\n", (int)command.count, command.data);
            return 1;
        }

        Value_Type type = VALUE_I64;
        (void)parse_scalar_value_type(command, &type);

        Value value = value_i64(0);

        if (!parse_explicit_value(consts, type, line, &value))
        {
            ERR("Invalid %s value or constant: %.*s\n", value_type_name(type), (int)line.count, line.data);
            return 1;
        }

        vm_push(vm, value);
    }
    else if (sv_eq_ignore_case(command, "pushs"))
    {
        if (raw_arg.count == 0)
        {
            ERR("pushs requires a string\n");
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
            ERR("convert requires a scalar target type\n");
            return 1;
        }

        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
            return 1;
        }

        if (!parse_scalar_value_type(line, &target_type))
        {
            ERR("convert only supports scalar target types: u8, i8, i16, u16, i32, u32, i64, u64, f32, f64\n");
            return 1;
        }

        Value src = vm_pop(vm);
        Value converted = value_i64(0);

        if (!value_convert_scalar(src, target_type, &converted))
        {
            ERR("convert only supports numeric scalar source values\n");
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
            ERR("pack requires a struct name\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, line, &def))
        {
            ERR("Unknown struct: %.*s\n", (int)line.count, line.data);
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
            ERR("get requires a struct name and index\n");
            return 1;
        }

        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, name, &def))
        {
            ERR("Unknown struct: %.*s\n", (int)name.count, name.data);
            return 1;
        }

        if (!parse_size_t_sv(line, &index))
        {
            ERR("get index must be a non-negative integer\n");
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
            ERR("set requires a struct name and index\n");
            return 1;
        }

        if (vm->count < 2)
        {
            ERR("There is less than 2 values on the stack\n");
            return 1;
        }

        if (!struct_def_lookup(struct_defs, name, &def))
        {
            ERR("Unknown struct: %.*s\n", (int)name.count, name.data);
            return 1;
        }

        if (!parse_size_t_sv(line, &index))
        {
            ERR("set index must be a non-negative integer\n");
            return 1;
        }

        if (!vm_set_field(vm, def, index))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "dlopen"))
    {
        String_View name = sv_chop_by_delim(&raw_arg, ' ');
        name = sv_trim(name);
        String_View path = sv_trim_left(raw_arg);

        if (name.count == 0 || path.count == 0)
        {
            ERR("dlopen requires a library name and path\n");
            return 1;
        }

        void *handle = vm_dlopen(temp_sv_to_cstr(path));
        if (handle == NULL)
        {
            ERR("dlopen failed for %.*s: %s\n", (int)path.count, path.data, vm_dl_last_error());
            return 1;
        }

        if (!dl_lib_define(dl_libs, name, handle))
        {
            (void)vm_dlclose(handle);
            return 1;
        }
    }
    else if (sv_eq_ignore_case(command, "dlsym"))
    {
        String_View sym_name = sv_chop_by_delim(&raw_arg, ' ');
        sym_name = sv_trim(sym_name);
        raw_arg = sv_trim_left(raw_arg);

        String_View lib_name = sv_chop_by_delim(&raw_arg, ' ');
        lib_name = sv_trim(lib_name);
        String_View native_name = sv_trim_left(raw_arg);

        Dl_Lib *lib = NULL;

        if (sym_name.count == 0 || lib_name.count == 0 || native_name.count == 0)
        {
            ERR("dlsym requires a symbol name, library name, and native symbol\n");
            return 1;
        }

        if (!dl_lib_lookup(dl_libs, lib_name, &lib))
        {
            ERR("Unknown dynamic library: %.*s\n", (int)lib_name.count, lib_name.data);
            return 1;
        }

        void *ptr = vm_dlsym(lib->handle, temp_sv_to_cstr(native_name));
        if (ptr == NULL)
        {
            ERR("dlsym failed for %.*s: %s\n", (int)native_name.count, native_name.data, vm_dl_last_error());
            return 1;
        }

        if (!dl_sym_define(dl_syms, sym_name, ptr, lib->handle))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "dlclose"))
    {
        Dl_Lib *lib = NULL;

        if (line.count == 0)
        {
            ERR("dlclose requires a library name\n");
            return 1;
        }

        if (!dl_lib_lookup(dl_libs, line, &lib))
        {
            ERR("Unknown dynamic library: %.*s\n", (int)line.count, line.data);
            return 1;
        }

        void *owner_handle = lib->handle;
        dl_syms_remove_by_owner(dl_syms, owner_handle);

        if (!vm_dlclose(owner_handle))
        {
            ERR("dlclose failed for %.*s: %s\n", (int)line.count, line.data, vm_dl_last_error());
            return 1;
        }

        free((void *)lib->name.data);
        size_t index = (size_t)(lib - dl_libs->items);
        for (size_t i = index + 1; i < dl_libs->count; i++)
        {
            dl_libs->items[i - 1] = dl_libs->items[i];
        }
        dl_libs->count--;
    }
    else if (sv_eq_ignore_case(command, "dlcall"))
    {
        String_View sym_name = sv_chop_by_delim(&line, ' ');
        sym_name = sv_trim(sym_name);
        line = sv_trim(line);

        String_View ret_token = sv_chop_by_delim(&line, ' ');
        ret_token = sv_trim(ret_token);
        line = sv_trim(line);

        Dl_Sym *sym = NULL;
        Dlcall_Type ret_type = {0};
        String_View *arg_tokens = NULL;
        size_t arg_count = 0;

        if (sym_name.count == 0 || ret_token.count == 0)
        {
            ERR("dlcall requires a symbol name and return type\n");
            return 1;
        }

        if (!dl_sym_lookup(dl_syms, sym_name, &sym))
        {
            ERR("Unknown dynamic symbol: %.*s\n", (int)sym_name.count, sym_name.data);
            return 1;
        }

        if (!dlcall_type_parse(ret_token, struct_defs, true, &ret_type))
        {
            ERR("Unknown dlcall return type: %.*s\n", (int)ret_token.count, ret_token.data);
            return 1;
        }

        size_t max_arg_tokens = line.count == 0 ? 1 : line.count;
        arg_tokens = calloc(max_arg_tokens, sizeof(*arg_tokens));
        if (arg_tokens == NULL)
        {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        while (line.count > 0)
        {
            String_View token = sv_chop_by_delim(&line, ' ');
            token = sv_trim(token);
            line = sv_trim(line);

            if (token.count == 0)
                continue;

            arg_tokens[arg_count++] = token;
        }

        bool ok = vm_dlcall(vm, struct_defs, sym->ptr, sym_name, ret_type, arg_tokens, arg_count);
        free(arg_tokens);
        dlcall_type_free(&ret_type);
        if (!ok)
            return 1;
    }
    else if (console && sv_eq_ignore_case(command, "const"))
    {
        String_View name = {0};
        Value value = value_i64(0);

        if (!parse_const_line(line, &name, &value))
        {
            ERR("const requires a type, name, and value\n");
            return 1;
        }

        if (!const_define(consts, name, value))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "pop") || sv_eq_ignore_case(command, "drop"))
    {
        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
            return 1;
        }

        Value dropped = vm_pop(vm);
        value_free(&dropped);
    }
    else if (sv_eq_ignore_case(command, "print"))
    {
        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
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
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_add(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "sub"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_sub(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "mul"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_mul(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "div"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_div(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "mod"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_mod(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "exp"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_exp(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "swap"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_swap(vm);
    }
    else if (sv_eq_ignore_case(command, "dup"))
    {
        if (vm->count == 0)
        {
            ERR("There needs to be at least one number on the stack\n");
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
            ERR("There needs to be at least one number on the stack\n");
            return 1;
        }

        if (!vm_neg(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "eq"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_eq(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "neq"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_neq(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "lt"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_lt(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "gt"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_gt(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "lte"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_lte(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "gte"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_gte(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "and"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_and(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "or"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!vm_or(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "not"))
    {
        if (vm->count == 0)
        {
            ERR("There needs to be at least one number on the stack\n");
            return 1;
        }

        if (!vm_not(vm))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "over"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
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
            ERR("There is less than 3 numbers on the stack\n");
            return 1;
        }

        vm_rot(vm);
    }
    else if (sv_eq_ignore_case(command, "read"))
    {
        if (vm->count == 0)
        {
            ERR("There needs to be at least one number on the stack\n");
            return 1;
        }

        if (!(vm_read(vm, memory)))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "write"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        if (!(vm_write(vm, memory)))
            return 1;
    }
    else if (sv_eq_ignore_case(command, "printc"))
    {
        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
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
            ERR("Stack is empty\n");
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
            ERR("Stack is empty\n");
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
            ERR("Stack is empty\n");
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
            ERR("Stack is empty\n");
            return 1;
        }

        if (!vm_emits(vm))
            return 1;

        if (console)
            printf("\n");
    }
    else if (sv_eq_ignore_case(command, "error"))
    {
        if (vm->count == 0)
        {
            ERR("Stack is empty\n");
            return 1;
        }

        if (!vm_error_output(vm))
            return 1;

        if (console)
            ERR("\n");
    }
    else if (sv_eq_ignore_case(command, "dup2"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_dup2(vm);
    }
    else if (sv_eq_ignore_case(command, "nip"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
            return 1;
        }

        vm_nip(vm);
    }
    else if (sv_eq_ignore_case(command, "tuck"))
    {
        if (vm->count < 2)
        {
            ERR("There is less than 2 numbers on the stack\n");
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
        ERR("Command not found\n");
    }

    return 1;
}

int exec_program(Vm *vm, String_View program, Consts *consts, Struct_Defs *struct_defs, Dl_Libs *dl_libs, Dl_Syms *dl_syms)
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
                ERR("label requires a name\n");
                return 1;
            }

            Label label = {rest, i + 1};

            da_append(&labels, label);
        }
    }

    size_t ip = 0;
    if (!label_lookup(labels, sv_from_cstr("_main"), &ip))
    {
        ERR("Missing entry point: label _main\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Must have 2 numbers on the stack\n");
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
                ERR("Call stack is empty\n");
                return 1;
            }

            ip = da_pop(&call_stack);
        }
        else
        {
            if (exec_line(vm, &memory, consts, struct_defs, dl_libs, dl_syms, false, line) == 2)
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

bool preproc_current_active(Preproc_Cond_Stack *stack)
{
    if (stack->count == 0)
        return true;

    return stack->items[stack->count - 1].active;
}

bool preprocess_file(const char *filepath, String_Builder *out, Consts *consts, Struct_Defs *struct_defs, Preproc_Defs *defs)
{
    String_Builder file = {0};
    Preproc_Cond_Stack cond_stack = {0};
    bool ok = true;

    if (!read_entire_file(filepath, &file))
    {
        ERR("Failed to read file: %s\n", filepath);
        return false;
    }

    String_View program = sb_to_sv(file);

    while (program.count > 0)
    {
        String_View line = sv_chop_by_delim(&program, '\n');
        String_View rest = {0};
        String_View command = {0};

        line = sv_trim_line_end(line);
        line = sv_trim_left(line);

        if (line.count == 0)
            continue;
        if (line.data[0] == '#')
            continue;

        rest = line;
        command = sv_chop_by_delim(&rest, ' ');
        command = sv_trim(command);
        rest = sv_trim(rest);

        if (command.count > 0 && command.data[0] == '@')
        {
            if (sv_eq_ignore_case(command, "@if"))
            {
                String_View name = sv_chop_by_delim(&rest, ' ');
                name = sv_trim(name);

                if (name.count == 0)
                {
                    ERR("@if requires a definition name\n");
                    ok = false;
                    break;
                }

                bool parent_active = preproc_current_active(&cond_stack);
                bool cond = false;

                if (parent_active)
                    cond = preproc_def_lookup(defs, name);

                Preproc_Cond_Frame frame = {
                    .parent_active = parent_active,
                    .active = parent_active && cond,
                    .branch_taken = parent_active && cond,
                    .saw_else = false,
                };
                da_append(&cond_stack, frame);
            }
            else if (sv_eq_ignore_case(command, "@elif"))
            {
                if (cond_stack.count == 0)
                {
                    ERR("@elif without matching @if\n");
                    ok = false;
                    break;
                }

                String_View name = sv_chop_by_delim(&rest, ' ');
                name = sv_trim(name);

                if (name.count == 0)
                {
                    ERR("@elif requires a definition name\n");
                    ok = false;
                    break;
                }

                Preproc_Cond_Frame *frame = &cond_stack.items[cond_stack.count - 1];
                if (frame->saw_else)
                {
                    ERR("@elif cannot appear after @else\n");
                    ok = false;
                    break;
                }

                if (!frame->parent_active || frame->branch_taken)
                {
                    frame->active = false;
                }
                else
                {
                    bool cond = preproc_def_lookup(defs, name);
                    frame->active = cond;
                    if (cond)
                        frame->branch_taken = true;
                }
            }
            else if (sv_eq_ignore_case(command, "@else"))
            {
                if (cond_stack.count == 0)
                {
                    ERR("@else without matching @if\n");
                    ok = false;
                    break;
                }

                Preproc_Cond_Frame *frame = &cond_stack.items[cond_stack.count - 1];
                if (frame->saw_else)
                {
                    ERR("Duplicate @else in conditional block\n");
                    ok = false;
                    break;
                }

                frame->saw_else = true;
                if (!frame->parent_active || frame->branch_taken)
                {
                    frame->active = false;
                }
                else
                {
                    frame->active = true;
                    frame->branch_taken = true;
                }
            }
            else if (sv_eq_ignore_case(command, "@endif"))
            {
                if (cond_stack.count == 0)
                {
                    ERR("@endif without matching @if\n");
                    ok = false;
                    break;
                }

                cond_stack.count--;
            }
            else if (sv_eq_ignore_case(command, "@define"))
            {
                if (!preproc_current_active(&cond_stack))
                    continue;

                String_View name = sv_chop_by_delim(&rest, ' ');
                name = sv_trim(name);

                if (name.count == 0)
                {
                    ERR("@define requires a definition name\n");
                    ok = false;
                    break;
                }

                if (!preproc_defs_define(defs, name))
                {
                    ok = false;
                    break;
                }
            }
            else if (sv_eq_ignore_case(command, "@undef"))
            {
                if (!preproc_current_active(&cond_stack))
                    continue;

                String_View name = sv_chop_by_delim(&rest, ' ');
                name = sv_trim(name);

                if (name.count == 0)
                {
                    ERR("@undef requires a definition name\n");
                    ok = false;
                    break;
                }

                preproc_defs_undef(defs, name);
            }
            else
            {
                ERR("Unknown preprocessor directive: %.*s\n", (int)command.count, command.data);
                ok = false;
                break;
            }

            continue;
        }

        if (!preproc_current_active(&cond_stack))
            continue;

        if (sv_eq_ignore_case(command, "include"))
        {
            if (rest.count == 0)
            {
                ERR("include requires a file\n");
                ok = false;
                break;
            }

            String_Builder include_path = {0};
            make_include_path(filepath, rest, &include_path);

            if (!preprocess_file(include_path.items, out, consts, struct_defs, defs))
            {
                sb_free(include_path);
                ok = false;
                break;
            }

            sb_free(include_path);
        }
        else if (sv_eq_ignore_case(command, "const"))
        {
            if (rest.count == 0)
            {
                ERR("const requires a type, name, and value\n");
                ok = false;
                break;
            }

            String_View name = {0};
            Value value = value_i64(0);

            if (!parse_const_line(rest, &name, &value))
            {
                ERR("const requires a valid typed value: %.*s\n", (int)rest.count, rest.data);
                ok = false;
                break;
            }

            if (!const_define(consts, name, value))
            {
                ok = false;
                break;
            }
        }
        else if (sv_eq_ignore_case(command, "struct"))
        {
            Struct_Def def = {0};

            if (!parse_struct_line(rest, struct_defs, &def))
            {
                ERR("struct requires a valid name and field list: %.*s\n", (int)rest.count, rest.data);
                ok = false;
                break;
            }

            if (!struct_def_define(struct_defs, def))
            {
                struct_def_free(&def);
                ok = false;
                break;
            }
        }
        else
        {
            sb_append_sv(out, line);
            da_append(out, '\n');
        }
    }

    if (ok && cond_stack.count != 0)
    {
        ERR("Unterminated @if block\n");
        ok = false;
    }

    da_free(cond_stack);
    sb_free(file);
    return ok;
}

int console(Vm *vm)
{
    Memory memory = {0};
    Consts consts = {0};
    Struct_Defs struct_defs = {0};
    Dl_Libs dl_libs = {0};
    Dl_Syms dl_syms = {0};
    for (;;)
    {
        char input[INPUT_BUFFER_SIZE];
        printf("pdvm> ");
        if (!fgets(input, sizeof(input), stdin))
            break;
        if (exec_line(vm, &memory, &consts, &struct_defs, &dl_libs, &dl_syms, true, sv_from_cstr(input)) == 2)
            break;
    }

    dl_syms_free(dl_syms);
    dl_libs_free(dl_libs);
    consts_free(consts);
    struct_defs_free(struct_defs);
    memory_free(memory);
    return 0;
}

int exec_file(Vm *vm, char *filepath)
{
    Consts consts = {0};
    Struct_Defs struct_defs = {0};
    Preproc_Defs preproc_defs = {0};
    Dl_Libs dl_libs = {0};
    Dl_Syms dl_syms = {0};
    String_Builder expanded = {0};

    if (!preproc_defs_add_host_builtins(&preproc_defs))
    {
        preproc_defs_free(preproc_defs);
        dl_syms_free(dl_syms);
        dl_libs_free(dl_libs);
        consts_free(consts);
        struct_defs_free(struct_defs);
        sb_free(expanded);
        return 1;
    }

    if (!preprocess_file(filepath, &expanded, &consts, &struct_defs, &preproc_defs))
    {
        ERR("Failed to preprocess file\n");
        preproc_defs_free(preproc_defs);
        dl_syms_free(dl_syms);
        dl_libs_free(dl_libs);
        consts_free(consts);
        struct_defs_free(struct_defs);
        sb_free(expanded);
        return 1;
    }

    int result = exec_program(vm, sb_to_sv(expanded), &consts, &struct_defs, &dl_libs, &dl_syms);

    preproc_defs_free(preproc_defs);
    dl_syms_free(dl_syms);
    dl_libs_free(dl_libs);
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
