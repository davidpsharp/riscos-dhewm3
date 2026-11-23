/* RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt). */
/* Force-included in every dhewm3 C and C++ file for RISC OS (riscos/toolchain.cmake). */
#ifndef RISCOS_COMPAT_H
#define RISCOS_COMPAT_H
#ifdef __cplusplus
/* UnixLib's ctype.h defines the classification functions as macros too, which breaks C++'s
   ::toupper(c) and std::isdigit(c). The functions are there; drop the macros. */
#include <ctype.h>
#undef isalnum
#undef isalpha
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit
#undef isblank
#undef isascii
#undef toupper
#undef tolower
#undef toascii
#endif
#endif
