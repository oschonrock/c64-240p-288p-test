#pragma once

// trye to keep `clangd` somewhat happy by ignoring some oscar64 features
#ifdef __clang__
#define __interrupt
#define __hwinterrupt
#define __noinline
#define __zeropage
#define __native
#define __striped
#define page 1
#define full 1
#define __asm
#define PSCI(str) str
#define SCRC(str) str
#else
// alternative syntax for string literals which is not illegal C
#define PSCI(str) p##str
#define SCRC(str) s##str
// correctly written code should not be penalised
#define noexcept
#endif

