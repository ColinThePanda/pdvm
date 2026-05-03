#ifndef FFICONFIG_H
#define FFICONFIG_H
#define STDC_HEADERS 1
#define HAVE_ALLOCA_H 0
#define HAVE_MEMCPY 1
#define HAVE_LONG_DOUBLE_VARIANT 0
#define HAVE_INT128 1
#ifdef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
#ifdef LIBFFI_ASM
#ifdef __APPLE__
#define FFI_HIDDEN(name) .private_extern name
#else
#define FFI_HIDDEN(name) .hidden name
#endif
#else
#define FFI_HIDDEN __attribute__ ((visibility ("hidden")))
#endif
#else
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
#endif
#endif
