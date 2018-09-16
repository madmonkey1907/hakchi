// #########################################################################

#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif
#include <bootimg.h>
#include <include/portable_endian.h>
#include "sha.h"

// #########################################################################

static int disableInfo=0;
#define error_printf(...) (fprintf(stderr,__VA_ARGS__))
#define info_printf(...) (disableInfo?0:fprintf(stdout,__VA_ARGS__))
static size_t kernelSize(const void*data);
int kernel(const char*in0);
typedef int (*FILEStream)(FILE*in,FILE*out,void*param);
#define PHY_INFO_MAGIC 0xaa55a5a5

// #########################################################################

int saveFile(const char*fileName,uint8_t*X,size_t fs)
{
    int rv=0;
    FILE*hf=fopen(fileName,"wb");
    if(hf)
    {
        if(fwrite(X,1,fs,hf)==fs)
        {
            rv=1;
        }
        fclose(hf);
    }
    return rv;
}

// #########################################################################

size_t loadFile(const char*fileName,uint8_t*X)
{
    size_t fs=0;
    FILE*hf=fopen(fileName,"rb");
    if(hf)
    {
        fseek(hf,0,SEEK_END);
        fs=ftell(hf);
        if(X)
        {
            fseek(hf,0,SEEK_SET);
            if(fread(X,1,fs,hf)!=fs)
            {
                fs=0;
            }
        }
        fclose(hf);
    }
    return fs;
}

// #########################################################################
// dd if=/dev/mapper/root-crypt | gzip | hexdump -v -e '64/1 "%02x""\n"'
int dehex(const char*in,const char*out)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        info_printf("input: %s, fs: %zu\n",in,fs);
        uint8_t b_in[fs+16];
        uint8_t b_out[fs+16];
        uint8_t b_tmp[4*1024];
        fs=loadFile(in,b_in);
        size_t outLen=0;
        size_t lineLen=0;
        uint8_t octet=0,value=0;
        int fileNumber=0;
        for(size_t i=0;i<fs;++i)
        {
            uint8_t c=b_in[i];
            switch(c)
            {
            case 'A'...'F':
                c=c-'A'+'a';
            case 'a'...'f':
                c=c-'a'+'0'+10;
            case '0'...'9':
                c=c-'0';
                octet=1-octet;
                value=(value<<4)|(c&0xf);
                if(octet==0)
                {
                    b_tmp[lineLen++]=value;
                }
                break;
            case '\n':
                if(lineLen)
                {
                    memcpy(b_out+outLen,b_tmp,lineLen);
                    outLen+=lineLen;
                }
            case ':':
                lineLen=0;
            case ' ':
                octet=0;
                break;
            default:
                if(outLen)
                {
                    char fout[1024];
                    sprintf(fout,"%s_%i.bin",out,++fileNumber);
                    info_printf("output: %s, fs: %zu\n",fout,outLen);
                    saveFile(fout,b_out,outLen);
                    outLen=0;
                }
                lineLen=0;
                octet=2;
                break;
            }
        }
    }
    return 0;
}

// #########################################################################

static int isKernel(const void*data)
{
    const boot_img_hdr*h=(const boot_img_hdr*)data;
    return (memcmp(h->magic,BOOT_MAGIC,BOOT_MAGIC_SIZE)==0)?1:0;
}

static size_t kernelSize(const void*data)
{
    size_t size=0;
    if(isKernel(data))
    {
        const boot_img_hdr*h=(const boot_img_hdr*)data;
        size_t pages=1;
        pages+=(h->kernel_size+h->page_size-1)/h->page_size;
        pages+=(h->ramdisk_size+h->page_size-1)/h->page_size;
        pages+=(h->second_size+h->page_size-1)/h->page_size;
        pages+=(h->dt_size+h->page_size-1)/h->page_size;
        size=pages*h->page_size;
    }
    return size;
}

// #########################################################################

struct chunk
{
    uint32_t offset;
    uint32_t size;
    uint32_t hsize;
};

struct bootimghash
{
    uint8_t id[2][SHA_DIGEST_SIZE];
    struct chunk c[5];
    uint32_t i;
    uint32_t match;
    SHA_CTX ctx;
};

void bcinit(struct bootimghash*b)
{
    memset(b,0,sizeof(*b));
}

void bcupdate(struct bootimghash*b,const void*data,uint32_t size)
{
    if(size==0)
        return;

    if(b->i==0)
    {
        bcinit(b);
        if(!isKernel(data))
            return;
        SHA_init(&b->ctx);
        const boot_img_hdr*h=(const boot_img_hdr*)data;
        memcpy(b->id[0],h->id,SHA_DIGEST_SIZE);
        size_t page=1;
        b->c[1].offset=page*h->page_size;
        b->c[1].size=h->kernel_size;
        b->c[1].hsize=b->c[1].size;
        page+=(b->c[1].size+h->page_size-1)/h->page_size;
        b->c[2].offset=page*h->page_size;
        b->c[2].size=h->ramdisk_size;
        b->c[2].hsize=b->c[2].size;
        page+=(b->c[2].size+h->page_size-1)/h->page_size;
        b->c[3].offset=page*h->page_size;
        b->c[3].size=h->second_size;
        b->c[3].hsize=b->c[3].size;
        page+=(b->c[3].size+h->page_size-1)/h->page_size;
        b->c[4].offset=page*h->page_size;
        b->c[4].size=h->dt_size;
        b->c[4].hsize=b->c[4].size;
        page+=(b->c[4].size+h->page_size-1)/h->page_size;
        b->c[0].size=page*h->page_size;
        b->i=1;
    }

    if(b->c[0].size==0)
        return;

    if(b->c[b->i].offset<b->c[0].offset)
    {
        error_printf("missed data chunk at 0x%x\n",b->c[b->i].offset);
        b->c[0].size=0;
    }

    const uint8_t*d=(const uint8_t*)data;
    while((size>0)&&(b->i<5))
    {
        if(b->c[b->i].size==0)
        {
            if((b->i<4)&&(b->c[b->i].hsize==0))
                SHA_update(&b->ctx,&b->c[b->i].hsize,4);
            ++b->i;
            continue;
        }
        uint32_t o=b->c[b->i].offset-b->c[0].offset;
        uint32_t s=size;
        if(o<s)
        {
            s-=o;
            s=(s<b->c[b->i].size)?s:b->c[b->i].size;
            SHA_update(&b->ctx,d+o,s);
            b->c[b->i].offset+=s;
            b->c[b->i].size-=s;
            if(b->c[b->i].size==0)
            {
                SHA_update(&b->ctx,&b->c[b->i].hsize,4);
                b->c[b->i].size=0;
            }
        }
        b->c[0].offset+=s;
        size-=s;
        d+=s;
    }

    if(b->i==5)
    {
        const uint8_t*sha=SHA_final(&b->ctx);
        memcpy(b->id[1],sha,SHA_DIGEST_SIZE);
        if(memcmp(b->id[0],b->id[1],SHA_DIGEST_SIZE)==0)
            b->match=1;
        b->c[0].size=0;
    }
}

// #########################################################################

int isNandInfo(const uint8_t*data)
{
    const uint32_t*data32=(const uint32_t*)data;
    return (le32toh(*data32)==0xaa55a5a5)?1:0;
}

int loffset(const uint8_t*data)
{
    if(memcmp(data+4,"eGON.BT",7)==0)
        return 4;
    if(memcmp(data+4,"uboot",6)==0)
        return 5;
    if(isNandInfo(data))
        return 1;
    return 0;
}

int coffset(const uint8_t*data)
{
    if(memcmp(data+4,"eGON.BT",7)==0)
        return 3;
    if(memcmp(data+4,"uboot",6)==0)
        return 3;
    if(isNandInfo(data))
        return 2;
    return 0;
}

uint32_t ubsize(const uint8_t*data)
{
    const int offset=loffset(data);
    const uint32_t*data32=(const uint32_t*)data;
    return (offset==0)?0:le32toh(data32[offset]);
}

uint32_t ubsum(const uint8_t*data)
{
    const int offset=coffset(data);
    const uint32_t*data32=(const uint32_t*)data;
    return (offset==0)?0:le32toh(data32[offset]);
}

uint32_t stamp_value(const uint8_t*data)
{
    if(isNandInfo(data))
        return 0xae15bc34;
    return 0x5f0a6c39;
}

int checksum(const char*in,int fix)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        info_printf("input: %s, fs: %.8zx\n",in,fs);
        uint8_t*data=(uint8_t*)malloc(fs+3);
        if(data==0)
            return 1;
        fs=loadFile(in,data);
        uint32_t*data32=(uint32_t*)data;

        if(isKernel(data))
        {
            struct bootimghash b;
            bcinit(&b);
            bcupdate(&b,data,fs);
            info_printf("checksum %s\n",b.match?"OK":"failed");
            free(data);
            if(b.match&&fix)
                kernel(in);
            return b.match?0:1;
        }

        if((fs<32)||(loffset(data)==0))
        {
            error_printf("header is not found\n");
            free(data);
            return 1;
        }

        uint32_t*hs=&data32[loffset(data)];
        uint32_t l=le32toh(*hs);
        l=(l>0x100)?l:fs;
        if((l>fs)||((l%4)!=0))
        {
            error_printf("bad length in header\n");
            free(data);
            return 1;
        }
        info_printf("header size: 0x%.8x\n",l);

        uint32_t c=stamp_value(data)-ubsum(data);
        for(uint32_t i=0;i<(l/4);++i)
            c+=le32toh(data32[i]);
        info_printf("checksum: header=0x%.8x file=0x%.8x\n",ubsum(data),c);

        uint32_t tl=l;
        if(isNandInfo(data+l))
            tl+=ubsize(data+l);

        if((c!=ubsum(data))||(l!=le32toh(*hs))||(tl!=fs))
        {
            if(fix!=0)
            {
                if(c!=ubsum(data))
                    info_printf("checksum updated\n");
                if((l!=le32toh(*hs))||(tl!=fs))
                    info_printf("length updated\n");
                data32[coffset(data)]=htole32(c);
                *hs=htole32(l);
                saveFile(in,data,tl);
                free(data);
                return 0;
            }
            if(c!=ubsum(data))
                error_printf("checksum check failed\n");
            if((l!=le32toh(*hs))||(tl!=fs))
                error_printf("wrong length\n");
            const int res=(c==ubsum(data))?0:1;
            free(data);
            return res;
        }

        info_printf("checksum OK\n");
        free(data);
        return 0;
    }
    return 1;
}

// #########################################################################

int split(const char*in)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        if(checksum(in,0))
            return 1;
        uint8_t*data=(uint8_t*)malloc(fs+4);
        if(data==0)
            return 1;
        fs=loadFile(in,data);
        uint32_t*data32=(uint32_t*)data;
        uint32_t offs=le32toh(data32[5]);
        if((offs<fs)&&isNandInfo(data+offs))
        {
            saveFile("nandinfo.bin",data+offs,fs-offs);
            fs=offs;
        }
        while(data[fs-1]==0xff)
            --fs;
        offs=le32toh(data32[6]);
        if((offs==0)||(offs>=fs))
        {
            free(data);
            return 1;
        }
        saveFile("script.bin",data+offs,fs-offs);
        fs=offs;
        while(data[fs-1]==0xff)
            --fs;
        data32[5]=htole32(fs);
        data32[6]=data32[5];
        saveFile(in,data,fs);
        free(data);
        checksum(in,1);
        return 0;
    }
    return 1;
}

// #########################################################################

int join(const char*in0,const char*in1,const char*in2)
{
    size_t fs0=loadFile(in0,0);
    size_t fs1=loadFile(in1,0);
    size_t fs2=loadFile(in2,0);
    if(fs0&&fs1)
    {
        if(checksum(in0,1))
            return 1;
        uint8_t*data=(uint8_t*)malloc((fs0+fs1+fs2)*2);
        if(data==0)
            return 1;
        fs0=loadFile(in0,data);
        uint32_t*data32=(uint32_t*)data;
        uint32_t offs=le32toh(data32[6]);
        if((offs!=0)&&(offs!=fs0))
        {
            free(data);
            return 1;
        }
        while(fs0&(0x10000-1))
            data[fs0++]=0xff;
        data32[6]=htole32(fs0);
        fs0+=loadFile(in1,data+fs0);
        while(fs0&(0x2000-1))
            data[fs0++]=0xff;
        data32[5]=htole32(fs0);
        if((fs2>0)&&(checksum(in2,0)==0))
        {
            fs0+=loadFile(in2,data+fs0);
            while(fs0&(0x2000-1))
                data[fs0++]=0;
        }
        saveFile(in0,data,fs0);
        free(data);
        return checksum(in0,1);
    }
    return 1;
}

// #########################################################################

int hsqs(const char*in0)
{
    size_t fs0=loadFile(in0,0);
    if(fs0)
    {
        info_printf("input: %s, fs: %zu\n",in0,fs0);
        uint8_t*data=(uint8_t*)malloc(fs0);
        if(data)
        {
            fs0=loadFile(in0,data);
            uint32_t*data32=(uint32_t*)data;
            if(le32toh(data32[0])==0x73717368)
            {
                uint32_t bs=0x1000;//le32toh(data32[3]);
                uint32_t fs1=le32toh(data32[0xa]);
                fs1=((fs1+bs-1)/bs)*bs;
                info_printf("output fs: %u\n",fs1);
                if(fs1<fs0)
                    saveFile(in0,data,fs1);
                free(data);
                return 0;
            }
            free(data);
        }
    }
    return 1;
}

// #########################################################################

#ifndef _WIN32
#pragma pack(push,1)

struct burn_param_b
{
    void*buffer;
    uint32_t length;
};

struct burn_param_t
{
    void*buffer;
    uint32_t length;
    uint32_t offset;
    union
    {
        uint32_t flags;
        struct
        {
            uint32_t raw:2;
            uint32_t getoob:1;
            uint32_t unused:29;
        } in;
        struct
        {
            uint32_t unused;
        } out;
    };
    struct burn_param_b oob;
    uint32_t badblocks;
};

struct hakchi_nandinfo
{
    char str[8];
    uint32_t size;
    uint32_t page_size;
    uint32_t pages_per_block;
    uint32_t block_count;
};

#pragma pack(pop)

enum
{
    ioctl_test=_IO('v',121),
    ioctl_burn_boot0=_IO('v',122),
    ioctl_burn_boot1=_IO('v',123),
    ioctl_read=_IO('v',124),
    ioctl_write=_IO('v',125),
    ioctl_read_boot0=_IO('v',126),
};

#define sector_size 0x20000u
#define uboot_base_f 0x100000u
#define kernel_base_f (sector_size*0x30u)
#define boot_area (sector_size*0x80u)

enum
{
    nandsize,
    _w_unused0,
    hakchi_test,
    nandinfo=hakchi_test,
    phy_write_force,
    phy_read,
    phy_write,
    log_read,
    log_write,
    read_boot0,
    burn_boot0,
    read_boot1,
    burn_boot1,
    read_uboot=read_boot1,
    burn_uboot=burn_boot1,
    read_boot2,
    burn_boot2,
    ramdisk,
    _w_unused1,
    cmdline,
    _w_unused2,
    max_cmd
};

const unsigned long _iocmd[max_cmd]={
    ioctl_test,
    ioctl_test,
    ioctl_test,
    ioctl_write,
    ioctl_read,
    ioctl_write,
    ioctl_read,
    ioctl_write,
    ioctl_read_boot0,
    ioctl_burn_boot0,
    ioctl_read,
    ioctl_write,//ioctl_burn_boot1,
    ioctl_read,
    ioctl_write,
    ioctl_read,
    ioctl_test,
    ioctl_read,
    ioctl_test,
};

const unsigned long _iocmd_param[max_cmd]={
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    0,
};

#define sf_str2cmd(x) {if(strcmp((cmdstr),(#x))==0)cmd=(x);}

int verify_test(const uint8_t*buffer,uint32_t cmd)
{
    if(memcmp(buffer,"hakchi",7))
        return 1;
    const struct hakchi_nandinfo*hni=(const struct hakchi_nandinfo*)buffer;
    int odi=disableInfo;
    disableInfo=0;
    if(cmd==nandinfo)
    {
        //info_printf("%u\n",hni->size);
        info_printf("%u ",hni->page_size);
        info_printf("%u ",hni->pages_per_block);
        info_printf("%u\n",hni->block_count);
    }
    if(cmd==nandsize)
    {
        info_printf("%x\n",hni->block_count);
    }
    disableInfo=odi;
    return 0;
}

int isRaw(uint32_t cmd)
{
    if(cmd==phy_read)
        return 1;
    if(cmd==phy_write)
        return 1;
    if(cmd==phy_write_force)
        return 1;
    return 0;
}

int sunxi_flash_ioctl(uint32_t cmd,uint32_t offs,uint32_t size,FILE*f)
{
    struct burn_param_t data;
    uint8_t buffer[sector_size];
    memset(&data,0,sizeof(data));
    data.buffer=buffer;
    const char*file="/dev/nanda";
    int fd=open(file,O_RDWR|O_NONBLOCK);
    if((fd<0)&&(errno==EPERM))
        fd=open(file,O_RDONLY|O_NONBLOCK);
    if(fd<0)
    {
        error(1,errno,"Cannot open %s: ",file);
        return 1;
    }
    data.length=sector_size;
    data.offset=0;
    const int testok=((ioctl(fd,ioctl_test,&data)<0)||(verify_test(buffer,cmd)))?1:0;
    if(testok)
        error_printf("ioctl test failed\n");
    const unsigned long iocmd=_iocmd[cmd];
    if((iocmd==ioctl_test)||(testok))
    {
        close(fd);
        return testok;
    }
    FILE*pf=0;
    int ret=0;
    const int dir=(cmd&1);//1 - write
    const int raw=isRaw(cmd);
    const int forced=(cmd==phy_write_force)?1:0;
    struct bootimghash b;
    bcinit(&b);
    uint32_t ubcsum=0;
    uint32_t sectorsWritten=0;
    if(dir)
    {
        size=size?:((size_t)-1);
        if(cmd==burn_boot1)
        {
            offs=offs?:uboot_base_f;
        }
        if(cmd==burn_boot2)
        {
            offs=offs?:kernel_base_f;
        }
        int needsector=1;
        size_t part=0;
        while((ret==0)&&(size>0))
        {
            if(!raw)
            {
                if(offs>=boot_area)
                {
                    error_printf("refusing to write beyond boot area at block %x\n",offs/sector_size);
                    ret=1;
                    break;
                }
            }
            if(needsector)
            {
                if(feof(f))
                    break;
                part=fread(buffer,1,sector_size,f);
                if((part<sector_size)&&(iocmd==ioctl_write))
                    while(part<sector_size)
                        buffer[part++]=0xff;
            }
            int needwrite=1;
            if(!forced)
            {
                uint8_t buffer2[sector_size];
                data.buffer=buffer2;
                data.length=sector_size;
                data.offset=offs;
                data.flags=0;
                data.in.raw=1;
                if(ioctl(fd,ioctl_read,&data)<0)
                {
                    error_printf("nand read failed at block %x\n",offs/sector_size);
                }
                else
                {
                    if(data.badblocks==0)
                        needwrite=memcmp(buffer,buffer2,sector_size);
                    else
                        error_printf("nand read badblock at %x\n",offs/sector_size);
                }
                data.buffer=buffer;
            }
            if(needwrite!=0)
            {
                data.length=part;
                data.offset=offs;
                data.flags=0;
                data.in.raw=forced?2:1;
                if(ioctl(fd,iocmd,&data)<0)
                {
                    error_printf("nand write failed at block %x\n",offs/sector_size);
                    ret=1;
                    break;
                }
                if(data.badblocks>0)
                {
                    data.badblocks=0;
                    error_printf("nand write badblock at %x\n",offs/sector_size);
                    if(!raw)
                    {
                        offs+=sector_size;
                        needsector=0;
                        continue;
                    }
                }
                ++sectorsWritten;
            }
            needsector=1;
            if(part>size)
                part=size;
            size-=part;
            offs+=part;
        }
    }
    else
    {
        size_t skip=0;
        int checkHeader=0;
        if(cmd==read_boot0)
        {
            checkHeader=1;
            size=32*1024;
            offs=0;
        }
        if(cmd==read_boot1)
        {
            checkHeader=1;
            size=size?:sector_size;
            offs=offs?:uboot_base_f;
        }
        if((cmd==read_boot2)||(cmd==ramdisk)||(cmd==cmdline))
        {
            checkHeader=1;
            size=size?:sector_size;
            offs=offs?:kernel_base_f;
        }
        while((ret==0)&&(size>0))
        {
            size_t part=(size<sector_size)?size:sector_size;
            data.length=part;
            data.offset=offs;
            data.flags=0;
            data.in.raw=raw?2:1;
            if((part<sector_size)&&(cmd!=read_boot0))
                data.length=sector_size;
            if(ioctl(fd,iocmd,&data)<0)
            {
                error_printf("nand read failed at block %x\n",offs/sector_size);
                ret=1;
                break;
            }
            if(data.badblocks>0)
            {
                error_printf("nand read badblock at %x\n",offs/sector_size);
                if(!raw)
                {
                    offs+=sector_size;
                    continue;
                }
            }
            if(skip>=part)
            {
                skip-=part;
                size-=part;
                offs+=part;
                continue;
            }
            if(checkHeader)
            {
                checkHeader=0;
                if((memcmp(buffer+4,"eGON.BT",7)==0)||(memcmp(buffer+4,"uboot",6)==0))
                {
                    size=ubsize(buffer);
                    ubcsum=stamp_value(buffer)-ubsum(buffer);
                    ubcsum-=ubsum(buffer);
                }
                if(isKernel(buffer))
                {
                    boot_img_hdr*h=(boot_img_hdr*)buffer;
                    size=kernelSize(buffer);
                    if(cmd==ramdisk)
                    {
                        size_t pages=1;
                        pages+=(h->kernel_size+h->page_size-1)/h->page_size;
                        pages*=h->page_size;
                        skip=pages-sector_size;
                        size=skip+h->ramdisk_size;
                        offs+=sector_size;
                        checkHeader=1;
                        continue;
                    }
                    if(cmd==cmdline)
                    {
                        skip=offsetof(boot_img_hdr,cmdline);
                        h->cmdline[sizeof(h->cmdline)-1]=0;
                        size=skip+strlen((const char*)h->cmdline);
                    }
                }
                if(cmd==ramdisk)
                {
                    const char*unp=0;
                    const uint32_t signature=*((uint32_t*)(buffer+skip));
                    if(signature==0x4f5a4c89)
                        unp="lzop -cd";
                    if(signature==0x587a37fd)
                        unp="xz -cd";
                    if(unp)
                    {
                        pf=popen(unp,"we");
                        if(pf)
                            f=pf;
                    }
                }
                part=(size<sector_size)?size:sector_size;
            }
            if(cmd==read_boot1)
            {
                if((size==part)&&isNandInfo(buffer+size))
                {
                    ubcsum+=stamp_value(buffer+size)-ubsum(buffer+size);
                    ubcsum-=ubsum(buffer+size);
                    size+=ubsize(buffer+size);
                    part=(size<sector_size)?size:sector_size;
                }
            }
            if((cmd==read_boot0)||(cmd==read_boot1))
            {
                const uint32_t*data32=(const uint32_t*)(buffer+skip);
                for(uint32_t i=0;i<((part-skip)/4);++i)
                {
                    ubcsum+=data32[i];
                }
            }
            if(cmd==read_boot2)
            {
                bcupdate(&b,buffer+skip,part-skip);
            }
            if(fwrite(buffer+skip,1,part-skip,f)!=(part-skip))
            {
                ret=1;
                break;
            }
            skip=0;
            size-=part;
            offs+=part;
        }
    }
    close(fd);
    if(pf)
        pclose(pf);
    if((cmd==read_boot0)||(cmd==read_boot1))
        if(ubcsum!=0)
            ret=1;
    if(cmd==read_boot2)
        if(b.match==0)
            ret=1;
    if(sectorsWritten)
        info_printf("nand sectors written: %u\n",sectorsWritten);
    return ret;
}

// #########################################################################

struct ioctlParam
{
    uint32_t cmd;
    uint32_t offs;
    uint32_t size;
};

int ioctlStream(FILE*in,FILE*out,void*param)
{
    struct ioctlParam*p=(struct ioctlParam*)param;
    const int dir=(p->cmd&1);//1 - write
    return sunxi_flash_ioctl(p->cmd,p->offs,p->size,dir?in:out);
}

// #########################################################################

int streamFile(const char*inFileName,const char*outFileName,FILEStream func,void*param)
{
    FILE*inhf=0,*inf=0;
    FILE*outhf=0,*outf=0;
    if(inFileName)
    {
        if(inFileName[0])
        {
            inf=inhf=fopen(inFileName,"rb");
            if(!inf)
            {
                error_printf("cannot open file: %s\n",inFileName);
                return 1;
            }
        }
    }
    else
    {
        inf=stdin;
        if(isatty(fileno(inf)))
        {
            error_printf("DO NOT WANT to %s terminal\n","read from");
            return 1;
        }
    }
    if(outFileName)
    {
        if(outFileName[0])
        {
            outf=outhf=fopen(outFileName,"wb");
            if(!outf)
            {
                error_printf("cannot open file: %s\n",outFileName);
                if(inhf)fclose(inhf);
                return 1;
            }
        }
    }
    else
    {
        outf=(disableInfo=1,stdout);
        if(isatty(fileno(outf)))
        {
            error_printf("DO NOT WANT to %s terminal\n","write to");
            if(inhf)fclose(inhf);
            return 1;
        }
    }
    const int ret=func(inf,outf,param);
    if(inhf)fclose(inhf);
    if(outhf)fclose(outhf);
    return ret;
}

// #########################################################################

int sunxi_flash(uint32_t cmd,uint32_t offs,uint32_t size,const char*fileName)
{
    const int dir=(cmd&1);//1 - write
    const char*dis="";
    if(cmd<=hakchi_test)
        fileName=dis;
    return streamFile(dir?fileName:dis,dir?dis:fileName,ioctlStream,&((struct ioctlParam){cmd,offs,size}));
}
#endif

// #########################################################################

int kernel(const char*in0)
{
    size_t fs0=loadFile(in0,0);
    if(fs0)
    {
        info_printf("input: %s, fs: %zu\n",in0,fs0);
        uint8_t*data=(uint8_t*)malloc(fs0);
        if(data)
        {
            fs0=loadFile(in0,data);
            size_t size=kernelSize(data);
            if(size<fs0)
            {
                fs0=size;
                info_printf("output fs: %zu\n",fs0);
                saveFile(in0,data,fs0);
                free(data);
                return 0;
            }
            else if(size==fs0)
            {
                free(data);
                return 0;
            }
            else
            {
                info_printf("size: %zu\n",size);
            }
            free(data);
        }
    }
    return 1;
}

// #########################################################################

#ifndef _WIN32
int devmemStream(FILE*in,FILE*out,void*param)
{
    int ret=1;
    struct ioctlParam*p=(struct ioctlParam*)param;
    int fd=open("/dev/mem",(in?O_RDWR:O_RDONLY)|O_SYNC);
    if(fd>0)
    {
        const int pageSize=getpagesize();
        const uint32_t startPage=p->offs&(~(pageSize-1));
        const uint32_t size=((p->offs+p->size+pageSize-1)&(~(pageSize-1)))-startPage;
        const uint32_t offset=p->offs&(pageSize-1);
        void*mapBase=mmap(0,size,(in?PROT_WRITE:0)|PROT_READ,MAP_SHARED,fd,startPage);
        if(mapBase==MAP_FAILED)
        {
            error_printf("mmap failed\n");
        }
        else
        {
            void*virtAddr=(char*)mapBase+offset;
            p->size=(p->size+3)/4;
            if(out)
                if(fwrite(virtAddr,4,p->size,out)==p->size)
                    ret=0;
            if(in)
                if(fread(virtAddr,4,p->size,in)==p->size)
                    ret=0;
            if(munmap(mapBase,size)<0)
                error_printf("munmap failed\n");
        }
        close(fd);
    }
    return ret;
}

int devmem(uint32_t write,uint32_t offs,uint32_t size,const char*fileName)
{
    const char*dis="";
    return streamFile(write?fileName:dis,write?dis:fileName,devmemStream,&((struct ioctlParam){write,offs,size}));
}
#endif

// #########################################################################

int main(int argc,const char*argv[])
{
    int ret=0;
    argv[0]=basename(argv[0]);
    for(int i=0;i<argc;++i)
    {
        if(strcmp(argv[i],"sntool")==0)
            continue;
        if(strcmp(argv[i],"dehex")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"capture";
            const char*out=(argc>(i+1))?(++i,argv[i]):"dehexout";
            ret+=dehex(in,out);
            continue;
        }
        if(strcmp(argv[i],"check")==0)
        {
            const int fix=((argc>(i+1))&&(strcmp(argv[i+1],"-f")==0))?++i:0;
            const char*in=(argc>(i+1))?(++i,argv[i]):"fes1.bin";
            ret+=in?checksum(in,fix):1;
            continue;
        }
        if(strcmp(argv[i],"split")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            ret+=split(in);
            continue;
        }
        if(strcmp(argv[i],"join")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            const char*in1=(argc>(i+1))?(++i,argv[i]):"script.bin";
            const char*in2=(argc>(i+1))?(++i,argv[i]):"nandinfo.bin";
            ret+=join(in0,in1,in2);
            continue;
        }
        if(strcmp(argv[i],"hsqs")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"rootfs.hsqs";
            ret+=hsqs(in0);
            continue;
        }
        if(strcmp(argv[i],"kernel")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"kernel.img";
            ret+=kernel(in0);
            continue;
        }
#ifndef _WIN32
        if((strcmp(argv[i],"sunxi_flash")==0)||(strcmp(argv[i],"sunxi-flash")==0))
        {
            const char*cmdstr=(argc>(i+1))?(++i,argv[i]):"0xff";
            uint32_t cmd=0xff;
            uint32_t offs=0;
            uint32_t size=0;
            sf_str2cmd(nandinfo);
            sf_str2cmd(nandsize);
            sf_str2cmd(phy_write_force);
            sf_str2cmd(phy_read);
            sf_str2cmd(phy_write);
            sf_str2cmd(log_read);
            sf_str2cmd(log_write);
            sf_str2cmd(read_boot0);
            sf_str2cmd(burn_boot0);
            sf_str2cmd(read_boot1);
            sf_str2cmd(burn_boot1);
            sf_str2cmd(read_uboot);
            sf_str2cmd(burn_uboot);
            sf_str2cmd(read_boot2);
            sf_str2cmd(burn_boot2);
            sf_str2cmd(ramdisk);
            sf_str2cmd(cmdline);
            const uint32_t params=(cmd<max_cmd)?_iocmd_param[cmd]:0;
            if(params>0)
            {
                offs=(argc>(i+1))?(++i,strtol(argv[i],0,16)):0;
                if(((cmd==read_boot1)||(cmd==burn_boot1))&&(offs<8))
                    offs=8+5*((offs?:1)-1);
                offs*=sector_size;
            }
            if(params>1)
            {
                size=(argc>(i+1))?(++i,strtol(argv[i],0,16)):0;
                size*=sector_size;
            }
            const char*in0=(argc>(i+1))?(++i,argv[i]):0;
            if(cmd==0xff)
                ret+=1;
            else
                ret+=sunxi_flash(cmd,offs,size,in0);
            continue;
        }
        if((strcmp(argv[i],"devmem")==0)||(strcmp(argv[i],"devmem-write")==0))
        {
            const int write=(strcmp(argv[i],"devmem")==0)?0:1;
            uint32_t offs=(argc>(i+1))?(++i,strtol(argv[i],0,0)):0;
            uint32_t size=(argc>(i+1))?(++i,strtol(argv[i],0,0)):0;
            const char*fn0=(argc>(i+1))?(++i,argv[i]):0;
            ret+=devmem(write,offs,size,fn0);
            continue;
        }
#endif
        error_printf("unknown command: %s\n",argv[i]);
        ret=1;
        break;
    }
    return ret;
}

// #########################################################################
