#include "v1util/base/platform.hpp"
#include "v1util/base/warnings.hpp"

V1_NO_WARNINGS
extern "C" {
#ifdef V1_OS_WIN
#  include "v1util/third-party/uu.spdr/src/spdr_win64_unit.c"
#elif V1_OS_POSIX
#  include "v1util/third-party/uu.spdr/src/spdr_posix_unit.c"
#endif
}
V1_RESTORE_WARNINGS
