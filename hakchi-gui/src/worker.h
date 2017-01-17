#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>
#include "fel.h"

class Worker:public QObject
{
    Q_OBJECT
public:
    enum Work
    {
        dumpUboot,
        dumpKernel,
        dumpNandFull,
        unpackKernel,
        packKernel,
        flashKernel,
        memboot,
        shutdown
    };
    explicit Worker(QObject*parent=0);
    ~Worker();

signals:
    void busy(bool b);
    void progress(int p);

public slots:
    void doWork(int work);

private slots:
    void calcProgress(int flow);

private:
    bool init();
    void do_dumpUboot();
    void do_dumpKernel();
    void do_dumpNandFull();
    void do_unpackKernel();
    void do_packKernel();
    void do_flashKernel();
    void do_memboot();
    void do_shutdown();

private:
    Fel*fel;
    int progressFlow;
    int progressTotal;
};

#endif // WORKER_H
