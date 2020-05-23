#pragma once

#include "platform.hpp"

#ifdef V1_OS_WIN
#include <malloc.h>
#define V1_ALLOCA(size) alloca(size)
#elif defined(V1_OS_FREEBSD)
#include <stdlib.h>
#define V1_ALLOCA(size) alloca(size);
#elif defined(V1_OS_LINUX)
#include <alloca.h>
#define V1_ALLOCA(size) alloca(size);
#else
#error "Don't know how to alloca() on your platform."
#endif
