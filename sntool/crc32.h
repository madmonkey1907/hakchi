#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void crc32fixup(void*data,size_t size,size_t pos,uint32_t crc);
uint32_t crc32(const void*data,size_t size);

#ifdef __cplusplus
}
#endif
