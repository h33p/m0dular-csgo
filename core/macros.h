#ifndef CSGO_MACROS_H
#define CSGO_MACROS_H

#ifdef DEBUG
#define DEVMSG(...) cvar->ConsoleDPrintf(__VA_ARGS__)
#define CDEVMSG(c, ...) auto col = c; cvar->ConsoleColorPrintf(col, __VA_ARGS__)
#else
#define DEVMSG(...)
#define CDEVMSG(...)
#endif

#endif
