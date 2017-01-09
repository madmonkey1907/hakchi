#include "fel.h"
#include <QScopedPointer>
extern "C" {
#include "fel_lib.h"
}
#include "md5int.h"
#include <stdio.h>

struct FeldevHandle:public feldev_handle{};
static const char*fastboot="fastboot_test";

struct spFree
{
    static inline void cleanup(void*pointer)
    {
        free(pointer);
    }
};

uboot_t::uboot_t()
{
    memset(&md5,0,sizeof(md5));
    cmdOffset=0;
}

void uboot_t::init(const QByteArray&ba)
{
    data=ba;
    md5calc(data.data(),data.size(),md5);
    cmdOffset=0;
    QByteArray pattern("bootcmd=");
    for(int i=0;i<(data.size()-pattern.size());i++)
    {
        if(memcmp(data.data()+i,pattern.data(),pattern.size())==0)
        {
            cmdOffset=i+pattern.size();
            cmd=QByteArray(data.data()+cmdOffset);
            break;
        }
    }
    if(cmdOffset==0)
    {
        *this=uboot_t();
    }
}

void uboot_t::doCmd(const char*str)
{
    size_t size=strlen(str);
    Q_ASSERT(size<(size_t)cmd.size());
    memset(data.data()+cmdOffset,0,cmd.size());
    memcpy(data.data()+cmdOffset,str,size);
}

Fel::Fel(QObject*parent):QObject(parent)
{
    dev=0;
    dramInitOk=false;
    feldev_init();
}

Fel::~Fel()
{
    feldev_done(dev);
}

bool Fel::init()
{
    if(dev==0)
    {
#if 0
        size_t size=0;
        QScopedPointer<feldev_list_entry,spFree>devs(list_fel_devices(&size));
        if(size==0)
        {
            return false;
        }
        if(size>1)
        {
            return false;
        }
#endif
        dev=static_cast<FeldevHandle*>(feldev_open(-1,-1,AW_USB_VENDOR_ID,AW_USB_PRODUCT_ID));
        while((dev!=0)&&(dev->soc_info->soc_id!=0x1667))
        {
            release();
            sleep(2);
            dev=static_cast<FeldevHandle*>(feldev_open(-1,-1,AW_USB_VENDOR_ID,AW_USB_PRODUCT_ID));
        }
    }
    return dev!=0;
}

bool Fel::initDram(bool force)
{
    if(dramInitOk&&!force)
        return true;

    uint8_t buf[0x80];
    if((size_t)fes1bin.size()<sizeof(buf))
        return false;
    if(!force)emit dataFlow(-sizeof(buf));
    if((force)||(readMemory(fes1_base_m+fes1bin.size()-sizeof(buf),sizeof(buf),buf)==sizeof(buf)))
    {
        if((!force)&&(memcmp(buf,fes1bin.data()+fes1bin.size()-sizeof(buf),sizeof(buf))==0))
        {
            dramInitOk=true;
            return true;
        }
        printf("uploading fes1.bin ...");
        emit dataFlow(-fes1bin.size());
        if(writeMemory(fes1_base_m,fes1bin.size(),fes1bin.data())==(size_t)fes1bin.size())
        {
            printf(" done\n");
            dramInitOk=true;
            return runCode(fes1_base_m,2);
        }
        printf(" failed\n");
    }
    return false;
}

void Fel::release()
{
    feldev_close(dev);
    dev=0;
}

void Fel::setFes1bin(const QByteArray&data)
{
    fes1bin=data;
}

void Fel::setUboot(const QByteArray&data)
{
    uboot.init(data);
}

bool Fel::haveUboot()const
{
    return uboot.cmdOffset>0;
}

bool Fel::runCode(uint32_t addr,uint32_t s)
{
    if(init())
    {
        aw_fel_execute(dev,addr);
        release();
        if(s==0xffffffff)
        {
            dramInitOk=false;
            return true;
        }
        sleep(s);
        for(int i=0;i<8;i++)
            if(init())
                break;
        return init();
    }
    return false;
}

bool Fel::runUbootCmd(const char*str,bool noreturn)
{
    if(init()&&haveUboot())
    {
        uboot.doCmd(str);
        uint8_t buf[0x20];
        emit dataFlow(-sizeof(buf));
        if(readMemory(uboot_base_m,sizeof(buf),buf)!=sizeof(buf))
            return false;
        if(memcmp(buf,uboot.data.data(),sizeof(buf))==0)
        {
            size_t size=(uboot.cmd.size()+3)/4;
            size*=4;
            emit dataFlow(-size);
            if(writeMemory(uboot_base_m+uboot.cmdOffset,size,uboot.data.data()+uboot.cmdOffset)!=size)
                return false;
        }
        else
        {
            printf("uploading uboot.bin ...");
            emit dataFlow(-uboot.data.size());
            if(writeMemory(uboot_base_m,uboot.data.size(),uboot.data.data())!=(size_t)uboot.data.size())
            {
                printf(" failed\n");
                return false;
            }
            printf(" done\n");
        }
        printf("%s\n",str);
        return runCode(uboot_base_m,noreturn?0xffffffff:10);
    }
    return false;
}

static const size_t maxTransfer=0x10000;

size_t Fel::readMemory(uint32_t addr,size_t size,void*buf)
{
    if(init())
    {
        if((addr>=dram_base)&&(!initDram()))
            return 0;
        size&=(~3);
        size_t transfer=size;
        while(transfer)
        {
            size_t b=qMin(transfer,maxTransfer);
            aw_fel_read(dev,addr,buf,b);
            emit dataFlow(b);
            addr+=b;
            buf=((uint8_t*)buf)+b;
            transfer-=b;
        }
        return size;
    }
    return 0;
}

size_t Fel::writeMemory(uint32_t addr,size_t size,void*buf)
{
    if(init())
    {
        if((addr>=dram_base)&&(!initDram()))
            return 0;
        size&=(~3);
        size_t transfer=size;
        while(transfer)
        {
            size_t b=qMin(transfer,maxTransfer);
            aw_fel_write(dev,buf,addr,b);
            emit dataFlow(b);
            addr+=b;
            buf=((uint8_t*)buf)+b;
            transfer-=b;
        }
        return size;
    }
    return 0;
}

size_t Fel::readFlash(uint32_t addr,size_t size,void*buf)
{
    if((!init())||(!haveUboot()))
        return 0;
    if(((size+addr%sector_size+sector_size-1)/sector_size)>flash_mem_size)
    {
        size_t sectors=(size+addr%sector_size+sector_size-1)/sector_size-flash_mem_size;
        size_t read=readFlash(addr,sectors*sector_size-addr%sector_size,buf);
        addr+=read;
        size-=read;
        buf=static_cast<uint8_t*>(buf)+read;
    }
    if(((size+addr%sector_size+sector_size-1)/sector_size)>flash_mem_size)
    {
        return 0;
    }
    char cmd[1024];
    sprintf(cmd,"sunxi_flash phy_read %x %x %zx;%s",flash_mem_base,addr/sector_size,(size+addr%sector_size+sector_size-1)/sector_size,fastboot);
    if(runUbootCmd(cmd))
    {
        return readMemory(flash_mem_base+addr%sector_size,size,buf);
    }
    return 0;
}

size_t Fel::writeFlash(uint32_t addr,size_t size,void*buf)
{
    if((!init())||(!haveUboot()))
        return 0;
    if((addr%sector_size)!=0)
        return 0;
    if((size%sector_size)!=0)
        return 0;
    if((size/sector_size)>flash_mem_size)
    {
        size_t sectors=(size/sector_size)-flash_mem_size;
        size_t write=writeFlash(addr,sectors*sector_size,buf);
        if((write%sector_size)!=0)
            return 0;
        addr+=write;
        size-=write;
        buf=static_cast<uint8_t*>(buf)+write;
    }
    if((size/sector_size)>flash_mem_size)
    {
        return 0;
    }
    if(writeMemory(flash_mem_base,size,buf)==size)
    {
        char cmd[1024];
        sprintf(cmd,"sunxi_flash phy_write %x %x %zx;%s",flash_mem_base,addr/sector_size,size/sector_size,fastboot);
        if(runUbootCmd(cmd))
            return size;
    }
    return 0;
}
