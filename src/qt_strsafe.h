#ifndef QUICK_TERMINAL_QT_STRSAFE_H
#define QUICK_TERMINAL_QT_STRSAFE_H

#ifdef QUICK_TERMINAL_STRSAFE_IMPLEMENTATION
#define __CRT_STRSAFE_IMPL
#else
#define __CRT__NO_INLINE
#endif

#include <strsafe.h>

#endif
