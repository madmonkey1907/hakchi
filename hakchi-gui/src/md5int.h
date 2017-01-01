#ifndef MD5INT_H
#define MD5INT_H

#include <stdint.h>

typedef struct
{
    uint8_t buffer[64];
    uint32_t state[4];
    uint32_t count[2];
} md5context;

#ifdef __cplusplus
extern "C" {
#endif

void md5init(md5context*);
void md5update(md5context*,const void*,uint32_t);
void md5final(md5context*,void*);
void md5calc(const void*,uint32_t,void*);
char*md5print(const void*md5,char*str);

#ifdef __cplusplus
}
#endif

#endif
