#include "crc32.h"
#include <assert.h>

//https://sar.informatik.hu-berlin.de/research/publications/SAR-PR-2006-05/SAR-PR-2006-05_.pdf

#define c_crc32Xor 0xffffffffu
#define c_crc32Poly 0xedb88320u

#define _crc32(x) (((x)>>1)^(c_crc32Poly&(~(((x)&1u)-1u))))
#define _crc32_i(x) (x)
#define _revcrc32(x) (((x)<<1)^(((c_crc32Poly<<1)+1u)&(~(((x>>31)&1u)-1u))))
#define _revcrc32_i(x) ((x)<<24)
#define _crc_s0(x,f) _##f(x)
#define _crc_s0_i(x,f) _##f##_i(x)
#define _crc_s1(x,f) _crc_s0(_crc_s0(x,f),f)
#define _crc_s2(x,f) _crc_s1(_crc_s1(x,f),f)
#define _crc_s3(x,s,f) _crc_s2(_crc_s2(_crc_s0_i(x,f),f),f)
#define _crc_s4(x,s,f) _crc_s3(x,s/4u,f),_crc_s3(x+s,s/4u,f),_crc_s3(x+2u*s,s/4u,f),_crc_s3(x+3u*s,s/4u,f)
#define _crc_s5(x,s,f) _crc_s4(x,s/4u,f),_crc_s4(x+s,s/4u,f),_crc_s4(x+2u*s,s/4u,f),_crc_s4(x+3u*s,s/4u,f)
#define _crc_s6(x,s,f) _crc_s5(x,s/4u,f),_crc_s5(x+s,s/4u,f),_crc_s5(x+2u*s,s/4u,f),_crc_s5(x+3u*s,s/4u,f)
#define _crc_sf(x,s,f) _crc_s6(x,s/4u,f),_crc_s6(x+s,s/4u,f),_crc_s6(x+2u*s,s/4u,f),_crc_s6(x+3u*s,s/4u,f)

static const uint32_t c_crc32Table[256]={_crc_sf(0u,64u,crc32)};
static const uint32_t c_revcrc32Table[256]={_crc_sf(0u,64u,revcrc32)};

static uint32_t crc32update(uint32_t v,const void*data,size_t size)
{
    const uint8_t*p=(const uint8_t*)data;
    for(;size>0;size--,p++)
    {
        v=c_crc32Table[(v^(*p))&0xffu]^(v>>8);
    }
    return v;
}

static uint32_t reverse_crc32update(uint32_t v,const void*data,size_t size)
{
    const uint8_t*p=(const uint8_t*)data;
    for(;size>0;size--)
    {
        v=c_revcrc32Table[v>>24]^p[size-1]^(v<<8);
    }
    return v;
}

static void store_uint32_le(uint32_t x,void*dv)
{
    uint8_t*data=(uint8_t*)dv;
    data[0]=x&0xffu;x>>=8;
    data[1]=x&0xffu;x>>=8;
    data[2]=x&0xffu;x>>=8;
    data[3]=x;
}

void crc32fixup(void*data,size_t size,size_t pos,uint32_t crc)
{
    uint8_t*p=(uint8_t*)data;
    assert(pos<=(size-4));
    store_uint32_le(crc32update(c_crc32Xor,data,pos),p+pos);
    store_uint32_le(reverse_crc32update(crc^c_crc32Xor,p+pos,size-pos),p+pos);
    assert((crc32update(c_crc32Xor,data,size)^c_crc32Xor)==crc);
}

uint32_t crc32(const void*data,size_t size)
{
    uint32_t result=crc32update(c_crc32Xor,data,size)^c_crc32Xor;
    return result;
}
