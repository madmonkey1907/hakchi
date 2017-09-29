#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class CThreadWaiter;

class MainWindow:public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget*parent=0);
    ~MainWindow();

signals:
    void doWork(int work);
    void readNext();

private slots:
    void progress(int p);
    void showMessage(const QString&msg);
    void on_actionDump_uboot_triggered();
    void on_actionDump_kernel_img_triggered();
    void on_actionUnpack_kernel_img_triggered();
    void on_actionRebuild_kernel_img_triggered();
    void on_actionFlash_kernel_triggered();
    void on_actionFlash_uboot_triggered();
    void on_actionMemboot_triggered();
    void on_actionShutdown_triggered();
    void on_actionDump_nand_triggered();
    void on_actionWrite_nand_triggered();

private:
    Ui::MainWindow*ui;
    CThreadWaiter*waiter[2];
};

#endif // MAINWINDOW_H
