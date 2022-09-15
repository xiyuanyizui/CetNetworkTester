#include "zqprotocoldialog.h"
#include "ui_zqprotocoldialog.h"

#include <QtSql>
#include <QDateTime>
#include <QMessageBox>

typedef struct {
    uint8_t       c_flags;                /* c_convert0 + c_convert1 = c_flags */
    uint8_t       c_convert0;             /* c_convert0 + c_convert2 = c_convert0 */
    uint8_t       c_convert1;
    uint8_t       c_convert2;
} ASMRULE_T;

static ASMRULE_T const sg_zqt_rules = {0x7E, 0x7D, 0x02, 0x01};

static uint8_t calXorSumcheck(const QByteArray &data)
{
    uint8_t result = 0;
    
    for (int i = 0; i < data.size(); ++i) {
        result ^= data.at(i);
    }

    return result;
}

static QByteArray assembleByRules(const QByteArray &data, const ASMRULE_T *rules)
{
    QByteArray assembled;

    assembled.append(rules->c_flags);
    for (int i = 0; i < data.size(); ++i) {
        if (rules->c_flags == data.at(i)) {
            assembled.append(rules->c_convert0);
            assembled.append(rules->c_convert1);
        } else if (rules->c_convert0 == data.at(i)) {
            assembled.append(rules->c_convert0);
            assembled.append(rules->c_convert2);
        } else {
            assembled.append(data.at(i));
        }
    }
    assembled.append(rules->c_flags);

    return assembled;
}

static QByteArray deassembleByRules(const QByteArray &data, const ASMRULE_T *rules)
{
    QByteArray deassembled;
    QByteArray temp = data;

    if (rules->c_flags == temp.at(0)) {
        temp.remove(0, 1);
    }

    if (rules->c_flags == temp.at(temp.size()-1)) {
        temp.remove(temp.size()-1, 1);
    }

    char predata = temp.at(0);
    for (int i = 1; i < temp.size(); ++i) {
        if (predata == rules->c_convert0 && temp.at(i) == rules->c_convert1) {
            deassembled.append(rules->c_flags);
        } else if (predata == rules->c_convert0 && temp.at(i) == rules->c_convert2) {
            deassembled.append(rules->c_convert0);
        } else {
            deassembled.append(predata);
        }
        predata = temp.at(i);
    }
    deassembled.append(predata);

    return deassembled;
}


ZqProtocolDialog::ZqProtocolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZqProtocolDialog),
    flowseq(0)
{
    ui->setupUi(this);

    setFixedSize(615, 295);

    ui->deviceIdLineEdit->setMaxLength(8);
    ui->slaveToDatabaseButton->setEnabled(false);

    connect(ui->deviceIdLineEdit, QLineEdit::textChanged, this, ZqProtocolDialog::doDeviceIdChanged);
    connect(ui->slaveToDatabaseButton, QPushButton::clicked, this, ZqProtocolDialog::doSaveToDatabase);

    // 创建 北斗车载终端重汽定制协议 表
    QSqlQuery query;
    query.exec("create table zqdevinfo(id int primary key, deviceId varchar, iccid varchar,"
               "pNum varchar, vin varchar, sim varchar, version varchar)");

   zqDBDlg = new ZqDatabaseDialog(this);
   connect(ui->openDatabaseButton, QPushButton::clicked, zqDBDlg, ZqDatabaseDialog::show);
   connect(ui->slaveToDatabaseButton, QPushButton::clicked, zqDBDlg, ZqDatabaseDialog::doDatabaseChanged);
}

ZqProtocolDialog::~ZqProtocolDialog()
{
    delete ui;
}

QByteArray ZqProtocolDialog::deassembleToHex(const QString &parse)
{
    return deassembleByRules(QByteArray::fromHex(parse.toLatin1()), &sg_zqt_rules);
}

void ZqProtocolDialog::showFrame(bool login, bool data)
{
    ui->loginFrameGroupBox->setEnabled(login);
    ui->dataFrameGroupBox->setEnabled(data);
    show();
}

QString ZqProtocolDialog::seqAddFrame(const QString &frame)
{
    QByteArray data = QByteArray::fromHex(frame.toLatin1());
    int seq = uint8_t(data.at(3)) << 8 | uint8_t(data.at(4));
    if (++seq > 0xFFFF) {
        seq = 1;
    }
    int size = data.size();
    data[3] = (seq >> 8) & 0xFF;
    data[4] = (seq >> 0) & 0xFF;
    data[size - 3] = (char)calXorSumcheck(data.mid(2, size - 5));

    return data.toHex().toUpper();
}

void ZqProtocolDialog::doDeviceIdChanged()
{
    ui->slaveToDatabaseButton->setEnabled(!ui->deviceIdLineEdit->text().isEmpty());

    QSqlQuery query;
    query.exec(QString("select * from zqdevinfo where deviceId='%1'")
            .arg(ui->deviceIdLineEdit->text()));
    if (query.next()) {
        QString iccid = query.value(2).toString();
        QString pNum = query.value(3).toString();
        QString vin = query.value(4).toString();
        QString sim = query.value(5).toString();
        QString version = query.value(6).toString();

        ui->iccidLineEdit->setText(iccid);
        ui->pNumLineEdit->setText(pNum);
        ui->vinLineEdit->setText(vin);
        ui->simLineEdit->setText(sim);
        ui->versionLineEdit->setText(version);

        ui->databaseTipLabel->setText(tr("提示：设备ID %1 在数据库中已存在！").arg(ui->deviceIdLineEdit->text()));
    } else {
        ui->iccidLineEdit->clear();
        ui->pNumLineEdit->clear();
        ui->vinLineEdit->clear();
        ui->simLineEdit->clear();
        ui->versionLineEdit->clear();

        ui->databaseTipLabel->setText(tr("提示：数据库中未找到设备ID %1 ").arg(ui->deviceIdLineEdit->text()));
    }

    emit baseInfoChanged(baseInfo());
}

void ZqProtocolDialog::doSaveToDatabase()
{
    int result = QMessageBox::question(this, tr("是否更新数据库"), 
                    tr("确定要更新到数据库中吗？\t\n若设备ID已存在则更新，否则插入！\t"), 
                    QMessageBox::Ok, QMessageBox::No);
    if (QMessageBox::No == result) {
        return;
    }
    
    QSqlQuery query;

    query.exec(QString("select deviceId from zqdevinfo where deviceId='%1'")
                .arg(ui->deviceIdLineEdit->text()));
    if (query.next()) { // 已存在则更新
        // 启动一个事务操作
        QSqlDatabase::database().transaction();
        bool rtn = query.exec(QString("update zqdevinfo set iccid='%1', pNum='%2',"
            "vin='%3', sim='%4', version='%5' where deviceId='%6'")
            .arg(ui->iccidLineEdit->text())
            .arg(ui->pNumLineEdit->text())
            .arg(ui->vinLineEdit->text())
            .arg(ui->simLineEdit->text())
            .arg(ui->versionLineEdit->text())
            .arg(ui->deviceIdLineEdit->text()));

        if (rtn) {
            // 执行完成，则提交该事务，这样才可继续其他事务的操作
            QSqlDatabase::database().commit();
            QMessageBox::information(this, tr("提示"), tr("更新数据库成功！"));
        } else {
            // 执行失败，则回滚
            QSqlDatabase::database().rollback();
        }
        
    }else { // 不存在则插入
        query.exec(QString("select count (*) from zqdevinfo"));
        query.next();
        int count = query.value(0).toInt();
        
        query.exec(QString("insert into zqdevinfo values(%1, '%2', '%3', '%4', '%5', '%6', '%7')")
                    .arg(count + 1)
                    .arg(ui->deviceIdLineEdit->text())
                    .arg(ui->iccidLineEdit->text())
                    .arg(ui->pNumLineEdit->text())
                    .arg(ui->vinLineEdit->text())
                    .arg(ui->simLineEdit->text())
                    .arg(ui->versionLineEdit->text()));
        QMessageBox::information(this, tr("提示"), tr("插入数据库成功！"));
        ui->databaseTipLabel->setText(tr("提示：设备ID %1 在数据库中已存在！").arg(ui->deviceIdLineEdit->text()));
    }
}

QString ZqProtocolDialog::baseInfo()
{
    QString info;
    info.append(tr("设备 ID：%1\n").arg(ui->deviceIdLineEdit->text()));
    info.append(tr("SIM卡号：%1\n").arg(ui->simLineEdit->text()));
    info.append(tr("车辆VIN：%1").arg(ui->vinLineEdit->text()));

    return info;
}

QByteArray ZqProtocolDialog::encapsZQFrame(uint16_t command, const QByteArray &data)
{
    QByteArray generate;

    if (++flowseq == 0xFFFF) {
        flowseq = 1;
    }

    generate.append(ZQ_FRAME_SOH);
    generate.append(ZQ_FRAME_DLE);
    generate.append(flowseq >> 8);          // 序列计数
    generate.append(flowseq >> 0);
    generate.append('T');
    generate.append(QByteArray::fromHex(ui->deviceIdLineEdit->text().toLatin1()));
    generate.append((char)0x00);            // 0x00：不压缩 0x08：GZIP压缩
    generate.append((char)0xFF);            // 车型
    generate.append(command >> 8);          // 命令
    generate.append(command >> 0);
    generate.append(data.size() >> 8);      // 数据的长度
    generate.append(data.size() >> 0);
    generate.append(data);                  // 数据
    generate.append(calXorSumcheck(generate.mid(1, generate.size() - 1)));
    generate.append(ZQ_FRAME_EOT);

    return assembleByRules(generate, &sg_zqt_rules).toHex().toUpper();
}

QByteArray ZqProtocolDialog::loginFrame()
{
    QByteArray data;
    data.append(ui->pNumLineEdit->text());
    data.append(',');
    data.append(ui->iccidLineEdit->text());
    data.append(',');
    data.append(ui->versionLineEdit->text());
    data.append(',');
    data.append(ui->vinLineEdit->text());
    data.append(',');
    data.append(ui->simLineEdit->text());
    
    return encapsZQFrame(UP_ZQ_LOG, data);
}

QByteArray ZqProtocolDialog::dataFrame()
{
    /**
        7E
        0110 0001 54(T) 50187033(设备号) 00FF FA01(命令) 0065(长度)
        01 62 04(版本) 210824085357(时间) 56(V)
        B0BCA211A6738D075209001D001E1C413955040825BBBA0101751403F1960000
        CBA0A200BF410000899401021530029FE21A00FFFFFFFF78990A002456000057
        6B2CFFFFFFFF692D8383563E19AB1EFFFF4301010B16FFFFFFFF078304
        7E
    */

    QString dateTime = QDateTime::currentDateTime().toString("yyMMddhhmmss");
    QString payload = "B0BCA211A6738D075209001D001E1C413955040825BBBA0101751403F1960000CBA0A200BF410000899401021530029FE21A00FFFFFFFF78990A0024560000576B2CFFFFFFFF692D8383563E19AB1EFFFF4301010B16FFFFFFFF07";
    QByteArray data;
    data.append((char)0x01);  // 单元体数量
    data.append((char)0x62);  // 每个单元体字节数
    data.append((char)0x04);  // 版本
    data.append(QByteArray::fromHex(dateTime.toLatin1()));
    data.append('V');
    data.append(QByteArray::fromHex(payload.toLatin1()));
    
    return encapsZQFrame(UP_ZQ_FA01, data);
}

