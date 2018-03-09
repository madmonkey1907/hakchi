// #########################################################################

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <sys/ioctl.h>
#include <bootimg.h>
#include <include/portable_endian.h>
#include "crc32.h"

// #########################################################################

static bool disableInfo=false;
#define error_printf(...) (fprintf(stderr,__VA_ARGS__))
#define info_printf(...) (disableInfo?0:fprintf(stdout,__VA_ARGS__))
static size_t kernelSize(const void*data);

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

int checksum(const char*in,int fix)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        info_printf("input: %s, fs: %.8x\n",in,fs);
        uint8_t data[fs+3];
        fs=loadFile(in,data);
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);

        if((fs<32)||((memcmp(data+4,"eGON.BT",7)!=0)&&(memcmp(data+4,"uboot",6)!=0)))
        {
            error_printf("header is not found\n");
            return 1;
        }

        uint32_t&hs=data32[(memcmp(data+4,"uboot",6)==0)?5:4];
        uint32_t l=le32toh(hs);
        l=(l>0x100)?l:fs;
        if((l>fs)||((l%4)!=0))
        {
            error_printf("bad length in header\n");
            return 1;
        }
        info_printf("header size: 0x%.8x\n",l);

        l/=4;
        uint32_t c=0x5F0A6C39-le32toh(data32[3])-le32toh(hs)+l*4;
        for(uint32_t i=0;i<l;++i)
            c+=le32toh(data32[i]);
        l*=4;
        info_printf("checksum: header=0x%.8x file=0x%.8x\n",le32toh(data32[3]),c);

        if((c!=le32toh(data32[3]))||(l!=le32toh(hs))||(l!=fs))
        {
            if(fix!=0)
            {
                if(c!=le32toh(data32[3]))
                    info_printf("checksum updated\n");
                if((l!=le32toh(hs))||(l!=fs))
                    info_printf("length updated\n");
                data32[3]=htole32(c);
                hs=htole32(l);
                saveFile(in,data,l);
                return 0;
            }
            if(c!=le32toh(data32[3]))
                error_printf("checksum check failed\n");
            if((l!=le32toh(hs))||(l!=fs))
                error_printf("wrong length\n");
            return (c==le32toh(data32[3]))?0:1;
        }

        info_printf("checksum OK\n");
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
        checksum(in,1);
        //info_printf("input: %s, fs: %.8x\n",in,fs);
        uint8_t data[fs+3];
        fs=loadFile(in,data);
        while(data[fs-1]==0xff)
            --fs;
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);
        uint32_t offs=le32toh(data32[6]);
        if((offs==0)||(offs>=fs))
            return 1;
        saveFile("script.bin",data+offs,fs-offs);
        fs=offs;
        while(data[fs-1]==0xff)
            --fs;
        data32[5]=htole32(fs);
        data32[6]=data32[5];
        saveFile(in,data,fs);
        checksum(in,1);
        return 0;
    }
    return 1;
}

// #########################################################################

int join(const char*in0,const char*in1)
{
    size_t fs0=loadFile(in0,0);
    size_t fs1=loadFile(in1,0);
    if(fs0&&fs1)
    {
        checksum(in0,1);
        //info_printf("input: %s, fs: %.8x\n",in0,fs0);
        //info_printf("input: %s, fs: %.8x\n",in1,fs1);
        uint8_t data[fs0*2+fs1];
        fs0=loadFile(in0,data);
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);
        uint32_t offs=le32toh(data32[6]);
        if((offs!=0)&&(offs!=fs0))
            return 1;
        while(fs0&(0x10000-1))
            data[fs0++]=0xff;
        data32[6]=htole32(fs0);
        fs0+=loadFile(in1,data+fs0);
        while(fs0&(0x2000-1))
            data[fs0++]=0xff;
        data32[5]=htole32(fs0);
        saveFile(in0,data,fs0);
        checksum(in0,1);
        return 0;
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
        uint8_t*data=static_cast<uint8_t*>(malloc(fs0));
        if(data)
        {
            fs0=loadFile(in0,data);
            uint32_t*data32=reinterpret_cast<uint32_t*>(data);
            if(le32toh(data32[0])==0x73717368)
            {
                uint32_t bs=0x1000;//le32toh(data32[3]);
                uint32_t fs1=le32toh(data32[0xa]);
                fs1=((fs1+bs-1)/bs)*bs;
                info_printf("output fs: %zu\n",fs1);
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

struct burn_param_t
{
    void*buffer;
    long length;
    long offset;
    long unused;
};

struct hakchi_nandinfo
{
    char str[8];
    uint32_t size;
    uint32_t page_size;
    uint32_t pages_per_block;
    uint32_t block_count;
};

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

enum
{
    hakchi_test,
    phy_write_if,
    phy_read,
    phy_write,
    read_boot0,
    burn_boot0,
    read_boot1,
    burn_boot1,
    read_uboot=read_boot1,
    burn_uboot=burn_boot1,
    read_boot2,
    burn_boot2,
    ramdisk,
    _w_unused0,
    cmdline,
    _w_unused1,
    max_ioctl
};

const unsigned long _iocmd[max_ioctl]={
    ioctl_test,
    ioctl_write,
    ioctl_read,
    ioctl_write,
    ioctl_read_boot0,
    ioctl_burn_boot0,
    ioctl_read,
    ioctl_burn_boot1,
    ioctl_read,
    ioctl_write,
    ioctl_read,
    ioctl_test,
    ioctl_read,
    ioctl_test,
};

#define sf_str2cmd(x) {if(strcmp((cmdstr),(#x))==0)cmd=(x);}

int sunxi_flash_ioctl(uint32_t cmd,uint32_t offs,uint32_t size,FILE*f)
{
    burn_param_t data;
    char buffer[sector_size];
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
    FILE*pf=0;
    int ret=0;
    const bool dir=(cmd&1);//1 - write
    const unsigned long iocmd=_iocmd[cmd];
    size_t skip=0;
    if(dir)
    {
        size=size?:size_t(-1);
        if(cmd==burn_boot2)
        {
            offs=offs?:kernel_base_f;
        }
        while((ret==0)&&(size>0)&&(!feof(f)))
        {
            uint32_t fcrc=0,crc=1;
            if(cmd==phy_write_if)
            {
                data.length=sector_size;
                data.offset=offs;
                if(ioctl(fd,ioctl_read,&data)>=0)
                {
                    fcrc=crc32(buffer,sector_size);
                }
            }
            size_t part=fread(buffer,1,sector_size,f);
            if((part<sector_size)&&(iocmd==ioctl_write))
                while(part<sector_size)
                    buffer[part++]=0xff;
            if(cmd==phy_write_if)
            {
                crc=crc32(buffer,part);
            }
            if(crc!=fcrc)
            {
                data.length=part;
                data.offset=offs;
                if(ioctl(fd,iocmd,&data)<0)
                {
                    ret=1;
                    break;
                }
            }
            if(part>size)
                part=size;
            size-=part;
            offs+=part;
        }
    }
    else
    {
        bool checkHeader=false;
        if(cmd==read_boot0)
        {
            checkHeader=true;
            size=32*1024;
            offs=0;
        }
        if(cmd==read_boot1)
        {
            checkHeader=true;
            size=size?:sector_size;
            offs=offs?:uboot_base_f;
        }
        if((cmd==read_boot2)||(cmd==ramdisk)||(cmd==cmdline))
        {
            checkHeader=true;
            size=size?:sector_size;
            offs=offs?:kernel_base_f;
        }
        while((ret==0)&&(size>0))
        {
            size_t part=(size<sector_size)?size:sector_size;
            data.length=part;
            data.offset=offs;
            if((part<sector_size)&&(cmd!=read_boot0))
                data.length=sector_size;
            if(ioctl(fd,iocmd,&data)<0)
            {
                ret=1;
                break;
            }
            if(checkHeader)
            {
                checkHeader=false;
                if((memcmp(buffer+4,"eGON.BT",7)==0)||(memcmp(buffer+4,"uboot",6)==0))
                {
                    uint32_t*data32=reinterpret_cast<uint32_t*>(buffer);
                    uint32_t l=le32toh(data32[(memcmp(buffer+4,"uboot",6)==0)?5:4]);
                    size=l;
                }
                boot_img_hdr*h=reinterpret_cast<boot_img_hdr*>(buffer);
                if(memcmp(h->magic,BOOT_MAGIC,BOOT_MAGIC_SIZE)==0)
                {
                    size=kernelSize(buffer);
                    if(cmd==ramdisk)
                    {
                        size_t pages=1;
                        pages+=(h->kernel_size+h->page_size-1)/h->page_size;
                        pages*=h->page_size;
                        skip=pages%sector_size;
                        size=h->ramdisk_size+skip;
                        const size_t adv=pages/sector_size;
                        if(!isatty(fileno(stdout)))
                        {
                            checkHeader=true;
                        }
                        if(adv>0)
                        {
                            offs+=adv*sector_size;
                            continue;
                        }
                    }
                    if(cmd==cmdline)
                    {
                        skip=offsetof(boot_img_hdr,cmdline);
                        h->cmdline[sizeof(h->cmdline)-1]=0;
                        size=strlen((const char*)h->cmdline)+skip;
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
    return ret;
}

int sunxi_flash(uint32_t cmd,uint32_t offs,uint32_t size,const char*fileName)
{
    const bool dir=(cmd&1);//1 - write
    FILE*hf=0,*f=0;
    if(fileName)
    {
        f=hf=fopen(fileName,dir?"rb":"wb");
        if(!f)
        {
            error_printf("cannot open file: %s\n",fileName);
            return 1;
        }
    }
    else
    {
        f=dir?stdin:(disableInfo=true,stdout);
        if(isatty(fileno(f)))
        {
            error_printf("DO NOT WANT to %s terminal\n",dir?"read from":"write to");
            return 1;
        }
    }
    const int ret=sunxi_flash_ioctl(cmd,offs,size,f);
    if(hf)
        fclose(hf);
    return ret;
}

// #########################################################################

static size_t kernelSize(const void*data)
{
    size_t size=0;
    const boot_img_hdr*h=reinterpret_cast<const boot_img_hdr*>(data);
    if(memcmp(h->magic,BOOT_MAGIC,BOOT_MAGIC_SIZE)==0)
    {
        size_t pages=1;
        pages+=(h->kernel_size+h->page_size-1)/h->page_size;
        pages+=(h->ramdisk_size+h->page_size-1)/h->page_size;
        pages+=(h->second_size+h->page_size-1)/h->page_size;
        pages+=(h->dt_size+h->page_size-1)/h->page_size;
        size=pages*h->page_size;
    }
    return size;
}

int kernel(const char*in0)
{
    size_t fs0=loadFile(in0,0);
    if(fs0)
    {
        info_printf("input: %s, fs: %zu\n",in0,fs0);
        uint8_t*data=static_cast<uint8_t*>(malloc(fs0));
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

int main(int argc,const char*argv[])
{
    int ret=0;
    for(int i=1;i<argc;++i)
    {
        if(strcmp(argv[i],"dehex")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"capture";
            const char*out=(argc>(i+1))?(++i,argv[i]):"dehexout";
            ret+=dehex(in,out);
        }
        if(strcmp(argv[i],"check")==0)
        {
            const int fix=((argc>(i+1))&&(strcmp(argv[i+1],"-f")==0))?++i:0;
            const char*in=(argc>(i+1))?(++i,argv[i]):"fes1.bin";
            ret+=in?checksum(in,fix):1;
        }
        if(strcmp(argv[i],"split")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            ret+=split(in);
        }
        if(strcmp(argv[i],"join")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            const char*in1=(argc>(i+1))?(++i,argv[i]):"script.bin";
            ret+=join(in0,in1);
        }
        if(strcmp(argv[i],"hsqs")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"rootfs.hsqs";
            ret+=hsqs(in0);
        }
        if(strcmp(argv[i],"kernel")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"kernel.img";
            ret+=kernel(in0);
        }
        if(strcmp(argv[i],"sunxi_flash")==0)
        {
            const char*cmdstr=(argc>(i+1))?(++i,argv[i]):"0xff";
            uint32_t cmd=0xff;
            uint32_t offs=0;
            uint32_t size=0;
            sf_str2cmd(phy_read);
            sf_str2cmd(phy_write);
            sf_str2cmd(phy_write_if);
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
            if(cmd<=phy_write)
            {
                offs=(argc>(i+1))?(++i,strtol(argv[i],0,16)):0;
                size=(argc>(i+1))?(++i,strtol(argv[i],0,16)):0;
                offs*=sector_size;
                size*=sector_size;
            }
            const char*in0=(argc>(i+1))?(++i,argv[i]):0;
            if(cmd==0xff)
                ret+=1;
            else
                ret+=sunxi_flash(cmd,offs,size,in0);
        }
    }
    return ret;
}

// #########################################################################
