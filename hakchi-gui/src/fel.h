#ifndef FEL_H
#define FEL_H

#include <stdint.h>
#include <unistd.h>
#include <QObject>
#include <QByteArray>

struct FeldevHandle;

struct uboot_t
{
    QByteArray data;
    uint8_t md5[16];
    QByteArray cmd;
    uint32_t cmdOffset;

    uboot_t();
    void init(const QByteArray&ba);
    void doCmd(const char*str);
};

#define fes1_base_m 0x2000u
#define dram_base 0x40000000u
#define uboot_base_m (dram_base+0x7000000u)
#define uboot_base_f 0x100000u
#define sector_size 0x20000u
#define uboot_maxsize_f (sector_size*0x10)
#define kernel_base_f (sector_size*0x30)
#define kernel_maxsize_f (sector_size*0x20)
#define transfer_base_m (dram_base+0x7400000u)
#define transfer_maxsize_m (dram_base+0x10000000u-transfer_base_m)
#define transfer_maxsize_f (transfer_maxsize_m/sector_size)

class Fel:public QObject
{
    Q_OBJECT
public:
    explicit Fel(QObject*parent=0);
    ~Fel();
    bool init();
    bool initDram(bool force=false);
    void release();
    void setFes1bin(const QByteArray&data);
    void setUboot(const QByteArray&data);
    bool haveUboot()const;
    bool runCode(uint32_t addr,uint32_t s);
    bool runUbootCmd(const char*str,bool noreturn=false);
    size_t readMemory(uint32_t addr,size_t size,void*buf);
    size_t writeMemory(uint32_t addr,size_t size,void*buf);
    size_t readFlash(uint32_t addr,size_t size,void*buf);
    size_t writeFlash(uint32_t addr,size_t size,void*buf);
signals:
    void dataFlow(int flow);
private:
    FeldevHandle*dev;
    QByteArray fes1bin;
    uboot_t uboot;
    bool dramInitOk;
};

#endif // FEL_H
