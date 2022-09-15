#ifndef ZQPROTOCOLDIALOG_H
#define ZQPROTOCOLDIALOG_H

#include <QDialog>

#include "zqdatabasedialog.h"

typedef uint8_t ZQ_Frame_t;
enum {
    ZQ_FRAME_SOH = 0x01,
    ZQ_FRAME_DLE = 0x10,
    ZQ_FRAME_EOT = 0x04,
    ZQ_HEART_ENQ = 0x05,
    ZQ_HEART_ACK = 0x06
};

#define UP_ZQ_LOG                       0xF205
#define UP_ZQ_FA01                      0xFA01

namespace Ui {
class ZqProtocolDialog;
}

class ZqProtocolDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZqProtocolDialog(QWidget *parent = 0);
    ~ZqProtocolDialog();

    QString baseInfo();
    QByteArray loginFrame();
    QByteArray dataFrame();
    void showFrame(bool login, bool data);
    QString seqAddFrame(const QString &frame);

    QByteArray deassembleToHex(const QString &parse);

signals:
    void baseInfoChanged(const QString &info);

private slots:
    void doDeviceIdChanged();
    void doSaveToDatabase();

private:
    QByteArray encapsZQFrame(uint16_t command, const QByteArray &data);

private:
    Ui::ZqProtocolDialog *ui;
    ZqDatabaseDialog *zqDBDlg;
    uint16_t flowseq;
};

#endif // ZQPROTOCOLDIALOG_H
