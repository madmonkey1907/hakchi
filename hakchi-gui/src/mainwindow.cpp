#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QThread>
#include <QPointer>
#include <QMessageBox>
#include <QElapsedTimer>
#include "worker.h"
#include "wincon.h"

class CThreadWaiter
{
public:
    CThreadWaiter(QObject*o)
    {
        obj=o;
        thread=new QThread();
        thread->start();
        o->moveToThread(thread);
        QObject::connect(o,SIGNAL(destroyed()),thread,SLOT(quit()));
    }
    ~CThreadWaiter()
    {
        QElapsedTimer timer;
        timer.start();
        static const qint64 maxWait=0x600,i1=0x100;
        if(!obj.isNull())
        {
            obj->deleteLater();
            while((!obj.isNull())&&(timer.elapsed()<maxWait))
                if(thread->wait(10))
                    break;
        }
        thread->quit();
        if(!thread->wait(qMax(maxWait-timer.elapsed(),i1)))
            exit(1);
        delete thread;
    }
private:
    QPointer<QObject> obj;
    QThread*thread;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->progressBar->setVisible(false);
    Worker*worker=new Worker();
    waiter[0]=new CThreadWaiter(worker);
    connect(worker,SIGNAL(busy(bool)),ui->mainToolBar,SLOT(setDisabled(bool)));
    connect(worker,SIGNAL(progress(int)),this,SLOT(progress(int)));
    connect(this,SIGNAL(doWork(int)),worker,SLOT(doWork(int)));

    CWinCon*winCon=new CWinCon();
    waiter[1]=new CThreadWaiter(winCon);
    connect(winCon,SIGNAL(msg(QString)),this,SLOT(showMessage(QString)));
    connect(this,SIGNAL(readNext()),winCon,SLOT(readOutput()));
    QTimer::singleShot(256,this,SIGNAL(readNext()));

    srand(0);
    printf("Knock, knock\n");
}

MainWindow::~MainWindow()
{
    disconnect(SIGNAL(readNext()));
    printf("bye!\n");
    delete waiter[0];
    delete waiter[1];
    delete ui;
}

void MainWindow::progress(int p)
{
    bool visible=false;
    if((p>0)&&(p<100))
        visible=true;
    if(ui->progressBar->isVisible()!=visible)
        ui->progressBar->setVisible(visible);
    if((visible)&&(ui->progressBar->value()!=p))
        ui->progressBar->setValue(p);
}

void MainWindow::showMessage(const QString&msg)
{
    static int mm[2]={0x18,0x60};
    static int knock=14;
    if(knock<32)
    {
        if(--knock==0)
        {
            knock=32;
            mm[0]/=6;
            mm[1]/=6;
            ui->plainTextEdit->clear();
        }
    }
    ui->plainTextEdit->moveCursor(QTextCursor::End);
    ui->plainTextEdit->insertPlainText(msg);
    QTimer::singleShot(mm[0]+(rand()%(mm[1]-mm[0])),this,SIGNAL(readNext()));
}

void MainWindow::on_actionDump_uboot_triggered()
{
    emit doWork(Worker::dumpUboot);
}

void MainWindow::on_actionDump_kernel_img_triggered()
{
    emit doWork(Worker::dumpKernel);
}

void MainWindow::on_actionUnpack_kernel_img_triggered()
{
    emit doWork(Worker::unpackKernel);
}

void MainWindow::on_actionRebuild_kernel_img_triggered()
{
    emit doWork(Worker::packKernel);
}

void MainWindow::on_actionFlash_kernel_triggered()
{
    if(QMessageBox::warning(this,"","sure?",QMessageBox::Yes|QMessageBox::Abort,QMessageBox::Abort)!=QMessageBox::Yes)
        return;
    emit doWork(Worker::flashKernel);
}

void MainWindow::on_actionMemboot_triggered()
{
    emit doWork(Worker::memboot);
}

void MainWindow::on_actionShutdown_triggered()
{
    emit doWork(Worker::shutdown);
}

void MainWindow::on_actionDump_nand_triggered()
{
    emit doWork(Worker::dumpNandFull);
}
