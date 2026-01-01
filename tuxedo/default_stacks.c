#include <tuxedo/types.h>

alignas(8) static u8 s_excptStack[0x4000];
alignas(8) static u8 s_mainStack[0x20000];

MK_WEAK void* __ppc_excpt_sp = &s_excptStack[sizeof(s_excptStack)];
MK_WEAK void* __ppc_main_sp = &s_mainStack[sizeof(s_mainStack)];
