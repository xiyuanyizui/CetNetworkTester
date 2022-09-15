#ifndef PARSECOMMANDDIALOG_H
#define PARSECOMMANDDIALOG_H

#include <QDialog>

#include "zqprotocoldialog.h"
#include "jtprotocoldialog.h"

namespace Ui {
class ParseCommandDialog;
}

class ParseCommandDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParseCommandDialog(QWidget *parent = nullptr);
    ~ParseCommandDialog();
    void setProtocol(const QString &protocol, QObject *obj = nullptr);

private:
    QString protocolParseJiaoTong2(QString &parse);
    QString jiaoTong2_parsedClientData(uint16_t command, const QString &data);
    QString jiaoTong2_parsedServerData(uint16_t command, const QString &data);
    QString jiaoTong2_parsedData(uint8_t dev,       uint16_t command, const QString &data);

    QString protocolParseZhongQi(QString &parse);
    QString zhongQi_parsedClientData(uint16_t command, const QString &data);
    QString zhongQi_parsedServerData(uint16_t command, const QString &data);
    QString zhongQi_parsedData(uint8_t dev,       uint16_t command, const QString &data);
    
    QString protocolParseG6HJ1239(QString &parse);
    QString g6HJ1239_SignatureParse(const QString &tabs, const QByteArray &signature);
    QString g6HJ1239_RealTimeInfoParse(const QByteArray &data);

    QString protocolParseNingDeShiDaiRDB(QString &parse);
    QString NingDeShiDaiRDB_DataParse(uint8_t aid, uint8_t mid, 
                    const QByteArray &data, QString &dispatch);

private slots:
    void doTextChanged();

private:
    Ui::ParseCommandDialog *ui;
    QString m_protocol;
    QObject *m_obj;
    JtProtocolDialog *jtProtDlg;
    ZqProtocolDialog *zqProtDlg;
};

#endif // PARSECOMMANDDIALOG_H
