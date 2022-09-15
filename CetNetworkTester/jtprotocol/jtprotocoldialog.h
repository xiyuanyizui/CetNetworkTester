#ifndef JTPROTOCOLDIALOG_H
#define JTPROTOCOLDIALOG_H

#include <QDialog>

#include "jtdatabasedialog.h"

#define UP_REG                          0x0100
#define UP_REG_LOGOUT                   0x0003
#define UP_AULOG                        0x0102
#define UP_POSITION                     0x0200
#define COMM_ACK                        0x0001

namespace Ui {
class JtProtocolDialog;
}

class JtProtocolDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JtProtocolDialog(QWidget *parent = 0);
    ~JtProtocolDialog();

    QString baseInfo();
    QByteArray regiterFrame();
    QByteArray loginFrame();
    QByteArray logoutFrame();
    void showFrame(bool regiter, bool login, bool logout);

    QString getAlarmFlag(uint32_t alarmflag);
    QString getStatus(uint32_t status);
    QString getProvinceId(uint16_t province);
    QByteArray deassembleToHex(const QString &parse);

signals:
    void baseInfoChanged(const QString &info);
    
private slots:
    void doColourChanged(int index);
    void doSimNumberChanged();
    void doSaveToDatabase();
    void doExtendButton();
    
private:
    QByteArray encapsJTFrame(uint16_t command, const QByteArray &data);
    
private:
    Ui::JtProtocolDialog *ui;
    JtDatabaseDialog *jtDBDlg;
    uint16_t flowseq;
};

#endif // JTPROTOCOLDIALOG_H
