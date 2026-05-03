#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

static bool ensure_dir(const char *path)
{
    if (file_exists(path)) return true;
    return mkdir_if_not_exists(path);
}

static bool write_libffi_ffi_h(const char *template_path, const char *output_path, const char *target_macro, const char *version_string, const char *version_number)
{
    bool result = false;
    String_Builder input = {0};
    String_Builder output = {0};

    if (!read_entire_file(template_path, &input)) goto defer;

    for (size_t i = 0; i < input.count; ++i)
    {
        char *cursor = input.items + i;

        if (strncmp(cursor, "@VERSION@", 9) == 0)
        {
            sb_append_cstr(&output, version_string);
            i += 8;
            continue;
        }

        if (strncmp(cursor, "@TARGET@", 8) == 0)
        {
            sb_append_cstr(&output, target_macro);
            i += 7;
            continue;
        }

        if (strncmp(cursor, "@HAVE_LONG_DOUBLE@", 18) == 0)
        {
            sb_append_cstr(&output, "1");
            i += 17;
            continue;
        }

        if (strncmp(cursor, "@FFI_VERSION_STRING@", 20) == 0)
        {
            sb_append_cstr(&output, version_string);
            i += 19;
            continue;
        }

        if (strncmp(cursor, "@FFI_VERSION_NUMBER@", 20) == 0)
        {
            sb_append_cstr(&output, version_number);
            i += 19;
            continue;
        }

        if (strncmp(cursor, "@FFI_EXEC_TRAMPOLINE_TABLE@", 27) == 0)
        {
            sb_append_cstr(&output, "0");
            i += 26;
            continue;
        }

        sb_append(&output, input.items[i]);
    }

    sb_append_null(&output);
    if (!write_entire_file(output_path, output.items, output.count - 1)) goto defer;

    result = true;

defer:
    da_free(input);
    da_free(output);
    return result;
}

static bool write_libffi_fficonfig_h(const char *output_path, bool windows, bool have_int128)
{
    String_Builder sb = {0};
    sb_append_cstr(&sb, "#ifndef FFICONFIG_H\n");
    sb_append_cstr(&sb, "#define FFICONFIG_H\n");
    sb_append_cstr(&sb, "#define STDC_HEADERS 1\n");
    sb_appendf(&sb, "#define HAVE_ALLOCA_H %d\n", windows ? 0 : 1);
    sb_append_cstr(&sb, "#define HAVE_MEMCPY 1\n");
    sb_append_cstr(&sb, "#define HAVE_LONG_DOUBLE_VARIANT 0\n");
    if (!windows)
    {
        sb_append_cstr(&sb, "#define HAVE_HIDDEN_VISIBILITY_ATTRIBUTE 1\n");
    }
    if (have_int128)
    {
        sb_append_cstr(&sb, "#define HAVE_INT128 1\n");
    }
    sb_append_cstr(&sb,
"#ifdef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE\n"
"#ifdef LIBFFI_ASM\n"
"#ifdef __APPLE__\n"
"#define FFI_HIDDEN(name) .private_extern name\n"
"#else\n"
"#define FFI_HIDDEN(name) .hidden name\n"
"#endif\n"
"#else\n"
"#define FFI_HIDDEN __attribute__ ((visibility (\"hidden\")))\n"
"#endif\n"
"#else\n"
"#ifdef LIBFFI_ASM\n"
"#define FFI_HIDDEN(name)\n"
"#else\n"
"#define FFI_HIDDEN\n"
"#endif\n"
"#endif\n"
"#endif\n");

    sb_append_null(&sb);
    bool ok = write_entire_file(output_path, sb.items, sb.count - 1);
    da_free(sb);
    return ok;
}

int main(int argc, char **argv)
{
    GO_REBUILD_URSELF(argc, argv);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--run") == 0)
            argv[i] = "-run";
        else if (strcmp(argv[i], "--release") == 0)
            argv[i] = "-release";
    }

    bool run = false;
    bool release = false;
    flag_bool_var(&run, "run", false, "Run the program after compilation.");
    flag_bool_var(&release, "release", false, "Build an optimized release binary.");
    if (!flag_parse(argc, argv))
    {
        flag_print_error(stderr);
        return 1;
    }

    Cmd cmd = {};
    cmd_append(&cmd, "gcc");
    if (release)
    {
        cmd_append(&cmd, "-O3", "-DNDEBUG");
    }
    else
    {
        cmd_append(&cmd, "-Wall", "-Wextra");
    }

    const char *libffi_root = "third_party/libffi";
    const char *libffi_template = "third_party/libffi/include/ffi.h.in";
    const char *libffi_build_dir = "build/libffi";
    const char *libffi_include_dir = "build/libffi/include";
    const char *libffi_ffi_h = "build/libffi/include/ffi.h";
    const char *libffi_fficonfig_h = "build/libffi/include/fficonfig.h";
    const char *libffi_version_string = "3.6.0-dev";
    const char *libffi_version_number = "30600";

    const char *ffi_target_macro = NULL;
    const char *ffi_arch_include = NULL;
    const char *ffi_sources[8] = {0};
    size_t ffi_source_count = 0;

    bool has_libffi = file_exists(libffi_root) && file_exists(libffi_template);
    bool have_int128 = false;

    if (has_libffi)
    {
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
        ffi_target_macro = "X86_WIN64";
        ffi_arch_include = "third_party/libffi/src/x86";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/prep_cif.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/types.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/ffiw64.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/win64.S";
        have_int128 = true;
#elif defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
        ffi_target_macro = "X86_WIN32";
        ffi_arch_include = "third_party/libffi/src/x86";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/prep_cif.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/types.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/ffi.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/sysv.S";
#elif !defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
        ffi_target_macro = "X86_64";
        ffi_arch_include = "third_party/libffi/src/x86";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/prep_cif.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/types.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/ffi64.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/unix64.S";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/ffiw64.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/win64.S";
        have_int128 = true;
#elif !defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
        ffi_target_macro = "X86";
        ffi_arch_include = "third_party/libffi/src/x86";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/prep_cif.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/types.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/ffi.c";
        ffi_sources[ffi_source_count++] = "third_party/libffi/src/x86/sysv.S";
#else
        has_libffi = false;
        nob_log(NOB_WARNING, "vendored libffi present, but nob does not yet know how to build it on this architecture");
#endif
    }

    if (has_libffi)
    {
        if (!ensure_dir("build")) return 1;
        if (!ensure_dir(libffi_build_dir)) return 1;
        if (!ensure_dir(libffi_include_dir)) return 1;
        if (!write_libffi_ffi_h(libffi_template, libffi_ffi_h, ffi_target_macro, libffi_version_string, libffi_version_number)) return 1;
        if (!write_libffi_fficonfig_h(libffi_fficonfig_h,
#ifdef _WIN32
            true,
#else
            false,
#endif
            have_int128)) return 1;

        cmd_append(&cmd, "-DPDVM_HAS_LIBFFI=1");
        cmd_append(&cmd, "-Ibuild/libffi/include");
        cmd_append(&cmd, "-Ithird_party/libffi/include");
        cmd_append(&cmd, "-I", ffi_arch_include);
    }
    else
    {
        nob_log(NOB_INFO, "vendored libffi source not usable here; building without dlcall support");
        cmd_append(&cmd, "-DPDVM_HAS_LIBFFI=0");
    }

    cmd_append(&cmd, "-opdvm", "pdvm.c");

    if (has_libffi)
    {
        for (size_t i = 0; i < ffi_source_count; ++i)
        {
            cmd_append(&cmd, ffi_sources[i]);
        }
    }

#ifndef _WIN32
    cmd_append(&cmd, "-ldl");
#endif

    if (!cmd_run(&cmd))
        return 1;

    if (run)
    {
        cmd_append(&cmd, "./pdvm");
        for (int i = 2; i < argc; i++)
        {
            cmd_append(&cmd, argv[i]);
        }
        if (!cmd_run(&cmd))
            return 1;
    }
}
