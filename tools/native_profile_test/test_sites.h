/* Test override: sites are filled at runtime from the test functions. */
#include <stdint.h>
typedef struct { uint32_t offset, w0, w1; } RioProfSite;
#define RIO_PROF_SITES 4
extern RioProfSite rio_prof_sites[RIO_PROF_SITES];
