#include "worker.h"
#include "fel.h"
#include <QDir>
#include "bootimg.h"
#include "md5int.h"
#include <include/portable_endian.h>

#ifdef WIN32
#include <windows.h>
#endif

static const QString kernelFile("dump/kernel.img");

#ifdef WIN32
int system_hidden(const char*cmd)
{
    int result=7;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si,0,sizeof(si));
    memset(&pi,0,sizeof(pi));
    si.cb=sizeof(STARTUPINFO);
    si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=(HANDLE)_get_osfhandle(_fileno(stdin));
    si.hStdOutput=(HANDLE)_get_osfhandle(_fileno(stdout));
    si.hStdError=si.hStdOutput;
    SetHandleInformation(si.hStdInput,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    SetHandleInformation(si.hStdOutput,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
    QString cmdl("cmd /c ");
    cmdl+=QString::fromLocal8Bit(cmd).replace('/','\\');
    if(CreateProcessA(getenv("COMSPEC"),cmdl.toLocal8Bit().data(),0,0,TRUE,NORMAL_PRIORITY_CLASS|CREATE_NO_WINDOW,0,0,&si,&pi))
    {
        CloseHandle(pi.hThread);
        WaitForSingleObject(pi.hProcess,INFINITE);
        GetExitCodeProcess(pi.hProcess,(LPDWORD)&result);
        CloseHandle(pi.hProcess);
    }
    return result;
}
#define system(x) system_hidden(x)
#endif

int saveFile(const char*fileName,void*X,size_t fs)
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

size_t loadFile(const char*fileName,void*X)
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

QByteArray loadFile(const char*fileName)
{
    size_t fs=loadFile(fileName,0);
    if(fs)
    {
        QByteArray data(fs,Qt::Uninitialized);
        if(loadFile(fileName,data.data())==fs)
            return data;
    }
    return QByteArray();
}

Worker::Worker(QObject*parent):QObject(parent)
{
    fel=0;
    progressFlow=0;
    progressTotal=0;
}

Worker::~Worker()
{
    delete fel;
}

void flushOutput()
{
    fflush(stdout);
    fflush(stderr);
#ifdef WIN32
    FlushFileBuffers((HANDLE)_get_osfhandle(STDOUT_FILENO));
    FlushFileBuffers((HANDLE)_get_osfhandle(STDERR_FILENO));
#endif
}

void Worker::doWork(int work)
{
    progressFlow=0;
    progressTotal=0;
    emit busy(true);
    emit progress(0);
    flushOutput();
    switch(work)
    {
    case dumpUboot:
        do_dumpUboot();
        break;
    case dumpKernel:
        do_dumpKernel();
        break;
    case unpackKernel:
        do_unpackKernel();
        break;
    case packKernel:
        do_packKernel();
        break;
    case flashKernel:
        do_flashKernel();
        break;
    case memboot:
        do_memboot();
        break;
    case shutdown:
        do_shutdown();
        break;
    default:
        Q_ASSERT(false);
        break;
    }
    printf("\n");
    flushOutput();
    emit progress(100);
    emit busy(false);
}

void Worker::calcProgress(int flow)
{
    if(flow<0)
    {
        progressTotal-=flow;
        return;
    }
    progressFlow+=flow;
    if((progressFlow>0)&&(progressTotal>0))
    {
        int p=qMin(100*progressFlow/progressTotal,100);
        emit progress(p);
    }
}

bool Worker::init()
{
    if(fel)
        return fel->init();
    fel=new Fel(this);
    connect(fel,SIGNAL(dataFlow(int)),this,SLOT(calcProgress(int)));
    QByteArray data=loadFile("data/fes1.bin");
    if(data.size())
        fel->setFes1bin(data);
    else
        printf("fes1.bin not found\n");
    data=loadFile("data/uboot.bin");
    if(data.size())
        fel->setUboot(data);
    else
        printf("uboot.bin not found\n");
    return fel->init();
}

void Worker::do_dumpUboot()
{
    if((!init())||(!fel->haveUboot()))
    {
        return;
    }
    size_t size=0;
    QByteArray buf(sector_size*6,Qt::Uninitialized);
    calcProgress(-buf.size());
    if(fel->readFlash(uboot_base_f,buf.size(),buf.data())==(size_t)buf.size())
        size=le32toh(*reinterpret_cast<uint32_t*>(buf.data()+0x14u));
    else
        return;
    if((size==0)||(size>kernel_max_size))
    {
        printf("uboot: invalid size in header\n");
        return;
    }
    if(size>(size_t)buf.size())
    {
        size_t oldSize=buf.size();
        buf.resize(size);
        calcProgress(-(size-oldSize));
        if(fel->readFlash(uboot_base_f+oldSize,size-oldSize,buf.data()+oldSize)!=(size-oldSize))
        {
            printf("uboot: read error\n");
            return;
        }
    }
    QDir(".").mkdir("dump");
    saveFile("dump/uboot.bin",buf.data(),size);
    uint8_t md5[16];
    md5calc(buf.data(),size,md5);
    char md5str[40];
    printf("%s\n",md5print(md5,md5str));
    printf("%s - OK\n",Q_FUNC_INFO);
}

static size_t kernelSize(const QByteArray&data)
{
    size_t size=0;
    const boot_img_hdr*h=reinterpret_cast<const boot_img_hdr*>(data.constData());
    if(memcmp(h->magic,BOOT_MAGIC,BOOT_MAGIC_SIZE)==0)
    {
        size_t pages=1;
        pages+=(h->kernel_size+h->page_size-1)/h->page_size;
        pages+=(h->ramdisk_size+h->page_size-1)/h->page_size;
        pages+=(h->second_size+h->page_size-1)/h->page_size;
        pages+=(h->dt_size+h->page_size-1)/h->page_size;
        size_t ks=pages*h->page_size;
        if(ks<=kernel_max_size)
            size=ks;
    }
    return size;
}

void Worker::do_dumpKernel()
{
    if((!init())||(!fel->haveUboot()))
    {
        return;
    }
    size_t size=0;
    QByteArray buf(sector_size*0x20,Qt::Uninitialized);
    calcProgress(-buf.size());
    if(fel->readFlash(kernel_base_f,buf.size(),buf.data())==(size_t)buf.size())
        size=kernelSize(buf);
    else
        return;
    if((size==0)||(size>kernel_max_size))
    {
        printf("kernel: invalid size in header\n");
        return;
    }
    if(size>(size_t)buf.size())
    {
        size_t oldSize=buf.size();
        buf.resize(size);
        calcProgress(-(size-oldSize));
        if(fel->readFlash(kernel_base_f+oldSize,size-oldSize,buf.data()+oldSize)!=(size-oldSize))
        {
            printf("kernel: read error\n");
            return;
        }
    }
    QDir(".").mkdir("dump");
    saveFile(kernelFile.toLocal8Bit(),buf.data(),size);
    uint8_t md5[16];
    md5calc(buf.data(),size,md5);
    char md5str[40];
    printf("%s\n",md5print(md5,md5str));
    printf("%s - OK\n",Q_FUNC_INFO);
}

void Worker::do_unpackKernel()
{
    if(::system(QString("extractimg \"%1\"").arg(kernelFile).toLocal8Bit())==0)
        printf("%s - OK\n",Q_FUNC_INFO);
}

void Worker::do_packKernel()
{
    if(::system(QString("makeimg kernel").toLocal8Bit())==0)
        printf("%s - OK\n",Q_FUNC_INFO);
}

void Worker::do_flashKernel()
{
    if((!init())||(!fel->haveUboot()))
    {
        return;
    }
    if(::system(QString("makeimg kernel notx").toLocal8Bit())!=0)
    {
        printf("cannot build kernel.img\n");
        return;
    }
    QByteArray k=loadFile("kernel.img");
    if(k.size()==0)
    {
        printf("kernel.img not found\n");
        return;
    }
    size_t ksize=kernelSize(k);
    if((ksize>(size_t)k.size())||((size_t)k.size()>kernel_max_flash_size))
    {
        printf("kernel: invalid size in header\n");
        return;
    }
    ksize=(ksize+sector_size-1)/sector_size;
    ksize=ksize*sector_size;
    if((size_t)k.size()!=ksize)
    {
        const size_t oldSize=k.size();
        k.resize(ksize);
        memset(k.data()+oldSize,0,k.size()-oldSize);
    }
    calcProgress(-k.size()*2);
    if(fel->writeFlash(kernel_base_f,k.size(),k.data())!=(size_t)k.size())
    {
        printf("kernel: write error\n");
        return;
    }
    printf("kernel: write ok\n");
    QByteArray baver(k.size(),Qt::Uninitialized);
    if(fel->readFlash(kernel_base_f,k.size(),baver.data())!=(size_t)k.size())
    {
        printf("kernel: read error\n");
        return;
    }
    if(memcmp(k.data(),baver.data(),k.size())==0)
        printf("kernel: verify ok\n");
    else
        printf("kernel: verify fuck\n");
#if 0
    if(fel->writeFlash(sector_size*(0x30+0x20-2),sector_size,k.data())==sector_size)
    {
        QByteArray baver(sector_size,Qt::Uninitialized);
        if(fel->readFlash(sector_size*(0x30+0x20-2),sector_size,baver.data())==sector_size)
        {
            if(memcmp(k.data(),baver.data(),sector_size)==0)
                f_printf("kernel: ok\n");
            else
                f_printf("kernel: fuck\n");
        }
        return;
    }
#endif
    char cmd[1024];
    sprintf(cmd,"boota %x",kernel_base_m);
    if(!fel->runUbootCmd(cmd,true))
    {
        printf("kernel: runcmd error\n");
        return;
    }
    printf("%s - OK\n",Q_FUNC_INFO);
}

void Worker::do_memboot()
{
    if((!init())||(!fel->haveUboot()))
    {
        return;
    }
    QByteArray k=loadFile("kernel.img");
    if(k.size()==0)
    {
        printf("kernel.img not found\n");
        return;
    }
    const size_t ksize=kernelSize(k);
    if((ksize>(size_t)k.size())||((size_t)k.size()>kernel_max_size))
    {
        printf("kernel: invalid size in header\n");
        return;
    }
    calcProgress(-k.size());
    if(fel->writeMemory(kernel_base_m,k.size(),k.data())!=(size_t)k.size())
    {
        printf("kernel: write error\n");
        return;
    }
    char cmd[1024];
    sprintf(cmd,"boota %x",kernel_base_m);
    if(!fel->runUbootCmd(cmd,true))
    {
        printf("kernel: runcmd error\n");
        return;
    }
    printf("%s - OK\n",Q_FUNC_INFO);
}

void Worker::do_shutdown()
{
    if((!init())||(!fel->haveUboot()))
    {
        return;
    }
    if(!fel->runUbootCmd("shutdown",true))
    {
        printf("shutdown: runcmd error\n");
        return;
    }
    printf("%s - OK\n",Q_FUNC_INFO);
}
