#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSslSocket>

#include "cetlicenseinterface.h"
#include "cetupdateinterface.h"
#include "zqprotocoldialog.h"
#include "jtprotocoldialog.h"
#include "parsecommanddialog.h"

namespace Ui {
class MainWindow;
}

class QSettings;
class QMessageBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void initDllPlugin();

    enum ProtocolType {
        PROTOCOL_UNKNOWN = 0,
        PROTOCOL_JT2_STANDARD,
        PROTOCOL_ZQ_CUSTOM,
        PROTOCOL_G6_HJ1239,
        PROTOCOL_NDSD_RDB
    };

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void doProtocolSelectChanged(int index);
    void displayError(QAbstractSocket::SocketError socketError);
    void doConnected();
    void doReadyRead();
    void doTimeoutConnect();
    void doTimeoutSend();
    
    void doZqGenerateButton();
    void doJtGenerateButton();

    void doZqProtocolDialog();
    void doJtProtocolDialog();
    
    void on_connectButton_clicked();

    void on_sendButton_clicked();

    void on_timingSendButton_clicked();

    void on_clearButton_clicked();

    void on_seqAddButton_clicked();

    void on_parseCommandButton_clicked();

private:
    QObject *loadDllPlugin(const QString &dllname);

private:
    Ui::MainWindow *ui;
    ProtocolType protType;
    CetLicenseInterface *m_cetLicIface;
    CetUpdateInterface *m_cetUpIface;
    ZqProtocolDialog *zqProtDlg;
    JtProtocolDialog *jtProtDlg;
    ParseCommandDialog *parseCmdDlg;
    QTcpSocket *socket;
    QTimer *conntimer;
    int connecttime;
    int connecttiptime;
    QMessageBox *connecttip;

    QTimer *sendtimer;
    
    int recvcount;
    int sendcount;
};

#endif // MAINWINDOW_H
