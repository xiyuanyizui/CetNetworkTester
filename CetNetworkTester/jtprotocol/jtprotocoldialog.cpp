#include "jtprotocoldialog.h"
#include "ui_jtprotocoldialog.h"

#include <QtSql>
#include <QMessageBox>

typedef struct {
    uint8_t       c_flags;                /* c_convert0 + c_convert1 = c_flags */
    uint8_t       c_convert0;             /* c_convert0 + c_convert2 = c_convert0 */
    uint8_t       c_convert1;
    uint8_t       c_convert2;
} ASMRULE_T;

static ASMRULE_T const sg_jtt_rules = {0x7E, 0x7D, 0x02, 0x01};

typedef struct {
    const char *region;
    int code;
} RegionCode_t;

static RegionCode_t sg_rc[] = {
        {"北京市-京",                   0x0B},
        {"天津市-津",                   0x0C},
        {"河北省-冀",                   0x0D},
        {"山西省-晋",                   0x0E},
        {"内蒙古自治区-蒙",                0x0F},
        {"辽宁省-辽",                   0x15},
        {"吉林省-吉",                   0x16},
        {"黑龙江省-黑",                  0x17},
        {"上海市-沪",                   0x1F},
        {"江苏省-苏",                   0x20},
        {"浙江省-浙",                   0x21},
        {"安徽省-皖",                   0x22},
        {"福建省-闽",                   0x23},
        {"江西省-赣",                   0x24},
        {"山东省-鲁",                   0x25},
        {"河南省-豫",                   0x29},
        {"湖北省-鄂",                   0x2A},
        {"湖南省-湘",                   0x2B},
        {"广东省-粤",                   0x2C},
        {"广西壮族自治区-桂",               0x2D},
        {"海南省-琼",                   0x2E},
        {"重庆市-渝",                   0x32},
        {"四川省-川",                   0x33},
        {"贵州省-贵",                   0x34},
        {"云南省-云",                   0x35},
        {"西藏自治区-藏",                 0x36},
        {"陕西省-陕",                   0x3D},
        {"甘肃省-甘",                   0x3E},
        {"青海省-青",                   0x3F},
        {"宁夏回族自治区-宁",               0x40},
        {"新疆维吾尔族自治区-新",             0x41}
};

static const char *sg_alarmflag[] = { "紧急报警", "超速报警", "疲劳驾驶", "危险预警", 
    "GNSS模块发生故障", "GNSS天线未接或被剪断", "GNSS天线短路", "终端主电源欠压",
    "终端主电源掉电", "终端LCD或显示器故障", "TTS模块故障", "摄像头故障", 
    "道路运输证IC卡模块故障", "超速预警", "疲劳驾驶预警", "保留", "保留", "保留", 
    "当天累计驾驶超时", "超时停车", "进出区域", "进出路线", "路段行驶时间不足/过长", 
    "路线偏离报警", "车辆VSS故障", "车辆油量异常", "车辆被盗(通过车辆防盗器)",
    "车辆非法点火", "车辆非法位移", "碰撞预警", "侧翻预警", "非法开门报警"
};

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

JtProtocolDialog::JtProtocolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::JtProtocolDialog),
    flowseq(0)
{
    ui->setupUi(this);

    //setFixedSize(700, 350);

    ui->provinceComboBox->addItem(tr("无"), 0);
    int len = sizeof(sg_rc)/sizeof(sg_rc[0]);
    for (int i = 0; i < len; ++i) {
        ui->provinceComboBox->addItem(sg_rc[i].region, sg_rc[i].code);
    }

    ui->companyidComboBox->addItem(tr("无"), 0);
    ui->companyidComboBox->addItem(tr("江淮"), "JAC");
    ui->companyidComboBox->addItem(tr("江铃"), "JMC");
    ui->companyidComboBox->addItem(tr("福田"), "FOTON");

    //ui->extendGroupBox->hide();
    ui->slaveToDatabaseButton->setEnabled(false);
    ui->simLineEdit->setMaxLength(15);

    connect(ui->extendButton, QPushButton::clicked, this, JtProtocolDialog::doExtendButton);

    //connect(ui->colourComboBox, static_cast<void (QComboBox:: *)(int)>(QComboBox::currentIndexChanged), 
    //    this, JtProtocolDialog::doColourChanged);
    connect(ui->simLineEdit, QLineEdit::textChanged, this, JtProtocolDialog::doSimNumberChanged);
    connect(ui->slaveToDatabaseButton, QPushButton::clicked, this, JtProtocolDialog::doSaveToDatabase);

    // 创建 交通部标准协议(第二版) 表
    QSqlQuery query;
    query.exec("create table jt2devinfo(id int primary key, sim varchar, province int,"
               "city varchar, manufacturerid varchar, devtype varchar, devid varchar, "
               "colour int, platevin varchar, companyid int, digest int, encrypt int,"
               "secretkey varchar, chipid varchar, chipmakers varchar, iccid varchar,"
               "authcode varchar)");

    jtDBDlg = new JtDatabaseDialog(this);
    connect(ui->openDatabaseButton, QPushButton::clicked, jtDBDlg, JtDatabaseDialog::show);
    connect(ui->slaveToDatabaseButton, QPushButton::clicked, jtDBDlg, JtDatabaseDialog::doDatabaseChanged);
}

JtProtocolDialog::~JtProtocolDialog()
{
    delete ui;
}

QString JtProtocolDialog::getAlarmFlag(uint32_t alarmflag)
{
    QString data;
    int len = sizeof(sg_alarmflag)/sizeof(sg_alarmflag[0]);
    for (int i = 0; i < len; ++i) {
        if ((alarmflag >> i) & 0x01) {
            data.append(sg_alarmflag[i]);
            data.append("|");
        }
    }

    return data;
}

QString JtProtocolDialog::getStatus(uint32_t status)
{
    QString data;
    ((status >> 0) & 0x01)? data.append("ACC开|") : data.append("ACC关|");
    ((status >> 1) & 0x01)? data.append("定位|") : data.append("未定位|");
    ((status >> 2) & 0x01)? data.append("南纬|") : data.append("北纬|");
    ((status >> 3) & 0x01)? data.append("西经|") : data.append("东经|");
    ((status >> 4) & 0x01)? data.append("停运状态|") : data.append("运营状态|");
    if (0x00 == ((status >> 8) & 0x03)) {
        data.append("空车|");
    } else if (0x01 == ((status >> 8) & 0x03)) {
        data.append("半载|");
    } else if (0x02 == ((status >> 8) & 0x03)) {
        data.append("保留|");
    } else if (0x03 == ((status >> 8) & 0x03)) {
        data.append("满载|");
    }
    ((status >> 18) & 0x01)? data.append("GPS定位|") : data.append("未GPS定位|");
    ((status >> 19) & 0x01)? data.append("北斗定位|") : data.append("未北斗定位|");

    return data;
}

QString JtProtocolDialog::getProvinceId(uint16_t province)
{
    int len = sizeof(sg_rc)/sizeof(sg_rc[0]);
    for (int i = 0; i < len; ++i) {
        if (province == sg_rc[i].code) {
            return sg_rc[i].region;
        }
    }
    return "未知";
}

QByteArray JtProtocolDialog::deassembleToHex(const QString &parse)
{
   return deassembleByRules(QByteArray::fromHex(parse.toLatin1()), &sg_jtt_rules);
}

void JtProtocolDialog::showFrame(bool regiter, bool login, bool logout)
{
    ui->regiterFrameGroupBox->setEnabled(regiter);
    ui->LoginFrameGroupBox->setEnabled(login);
    ui->logoutFrameGroupBox->setEnabled(logout);
    show();
}

void JtProtocolDialog::doColourChanged(int index)
{
    if (index > 0) {
        ui->platevinLineEdit->setEnabled(true);
    } else {
        ui->platevinLineEdit->setEnabled(false);
    }
}

void JtProtocolDialog::doSimNumberChanged()
{
    ui->slaveToDatabaseButton->setEnabled(!ui->simLineEdit->text().isEmpty());

    QSqlQuery query;
    query.exec(QString("select * from jt2devinfo where sim='%1'")
            .arg(ui->simLineEdit->text()));
    if (query.next()) {
        int province = query.value(2).toInt();
        QString city = query.value(3).toString();
        QString manufacturerid = query.value(4).toString();
        QString devtype = query.value(5).toString();
        QString devid = query.value(6).toString();
        int colour = query.value(7).toInt();
        QString platevin = query.value(8).toString();
        int companyid = query.value(9).toInt();
        int digest = query.value(10).toInt();
        int encrypt = query.value(11).toInt();
        QString secretkey = query.value(12).toString();
        QString chipid = query.value(13).toString();
        QString chipmakers = query.value(14).toString();
        QString iccid = query.value(15).toString();
        QString authcode = query.value(16).toString();

        ui->provinceComboBox->setCurrentIndex(province);
        ui->cityLineEdit->setText(city);
        ui->manufactureridLineEdit->setText(manufacturerid);
        ui->devtypeLineEdit->setText(devtype);
        ui->devidLineEdit->setText(devid);
        ui->colourComboBox->setCurrentIndex(colour);
        ui->platevinLineEdit->setText(platevin);
        ui->companyidComboBox->setCurrentIndex(companyid);
        ui->digestComboBox->setCurrentIndex(digest);
        ui->encryptComboBox->setCurrentIndex(encrypt);
        ui->secretkeyPlainTextEdit->setPlainText(secretkey);
        ui->chipidLineEdit->setText(chipid);
        ui->chipmakersLineEdit->setText(chipmakers);
        ui->iccidLineEdit->setText(iccid);
        ui->authcodeLineEdit->setText(authcode);

        ui->databaseTipLabel->setText(tr("提示：SIM卡号 %1 在数据库中已存在！").arg(ui->simLineEdit->text()));
    } else {
        ui->provinceComboBox->setCurrentIndex(0);
        ui->cityLineEdit->clear();
        ui->manufactureridLineEdit->clear();
        ui->devtypeLineEdit->clear();
        ui->devidLineEdit->clear();
        ui->colourComboBox->setCurrentIndex(0);
        ui->platevinLineEdit->clear();
        
        ui->companyidComboBox->setCurrentIndex(0);
        ui->digestComboBox->setCurrentIndex(0);
        ui->encryptComboBox->setCurrentIndex(0);
        ui->secretkeyPlainTextEdit->clear();
        ui->chipidLineEdit->clear();
        ui->chipmakersLineEdit->clear();
        ui->iccidLineEdit->clear();
        ui->authcodeLineEdit->clear();

        ui->databaseTipLabel->setText(tr("提示：数据库中未找到SIM卡号 %1 ").arg(ui->simLineEdit->text()));
    }

    emit baseInfoChanged(baseInfo());
}

void JtProtocolDialog::doSaveToDatabase()
{
    int result = QMessageBox::question(this, tr("是否更新数据库"), 
                    tr("确定要更新到数据库中吗？\t\n若SIM卡号已存在则更新，否则插入！\t"), 
                    QMessageBox::Ok, QMessageBox::No);
    if (QMessageBox::No == result) {
        return;
    }
    
    QSqlQuery query;

    query.exec(QString("select sim from jt2devinfo where sim='%1'").arg(ui->simLineEdit->text()));
    if (query.next()) { // 已存在则更新
        // 启动一个事务操作
        QSqlDatabase::database().transaction();
        bool rtn = query.exec(QString("update jt2devinfo set province=%1, city='%2',"
            "manufacturerid='%3', devtype='%4', devid='%5', colour=%6, platevin='%7',"
            "companyid=%8, digest=%9, encrypt=%10, secretkey='%11', chipid='%12',"
            "chipmakers='%13', iccid='%14', authcode='%15' where sim='%16'")
            .arg(ui->provinceComboBox->currentIndex())
            .arg(ui->cityLineEdit->text())
            .arg(ui->manufactureridLineEdit->text())
            .arg(ui->devtypeLineEdit->text())
            .arg(ui->devidLineEdit->text())
            .arg(ui->colourComboBox->currentIndex())
            .arg(ui->platevinLineEdit->text())
            .arg(ui->companyidComboBox->currentIndex())
            .arg(ui->digestComboBox->currentIndex())
            .arg(ui->encryptComboBox->currentIndex())
            .arg(ui->secretkeyPlainTextEdit ->toPlainText())
            .arg(ui->chipidLineEdit->text())
            .arg(ui->chipmakersLineEdit->text())
            .arg(ui->iccidLineEdit->text())
            .arg(ui->authcodeLineEdit->text())
            .arg(ui->simLineEdit->text()));

        if (rtn) {
            // 执行完成，则提交该事务，这样才可继续其他事务的操作
            QSqlDatabase::database().commit();
            QMessageBox::information(this, tr("提示"), tr("更新数据库成功！"));
        } else {
            // 执行失败，则回滚
            QSqlDatabase::database().rollback();
        }
    }else { // 不存在则插入
        query.exec(QString("select count (*) from jt2devinfo"));
        query.next();
        int count = query.value(0).toInt();
        
        query.exec(QString("insert into jt2devinfo values(%1, '%2', %3, '%4', '%5', '%6',"
                    "'%7', %8, '%9', %10, %11, %12, '%13', '%14', '%15', '%16', '%17')")
                    .arg(count + 1)
                    .arg(ui->simLineEdit->text())
                    .arg(ui->provinceComboBox->currentIndex())
                    .arg(ui->cityLineEdit->text())
                    .arg(ui->manufactureridLineEdit->text())
                    .arg(ui->devtypeLineEdit->text())
                    .arg(ui->devidLineEdit->text())
                    .arg(ui->colourComboBox->currentIndex())
                    .arg(ui->platevinLineEdit->text())
                    .arg(ui->companyidComboBox->currentIndex())
                    .arg(ui->digestComboBox->currentIndex())
                    .arg(ui->encryptComboBox->currentIndex())
                    .arg(ui->secretkeyPlainTextEdit ->toPlainText())
                    .arg(ui->chipidLineEdit->text())
                    .arg(ui->chipmakersLineEdit->text())
                    .arg(ui->iccidLineEdit->text())
                    .arg(ui->authcodeLineEdit->text()));
        QMessageBox::information(this, tr("提示"), tr("插入数据库成功！"));
        ui->databaseTipLabel->setText(tr("提示：SIM卡号 %1 在数据库中已存在！").arg(ui->simLineEdit->text()));
    }
}

void JtProtocolDialog::doExtendButton()
{
    if ("开启扩展" == ui->extendButton->text()) {
        ui->extendButton->setText(tr("关闭扩展"));
        ui->extendGroupBox->show();
    } else {
        ui->extendButton->setText(tr("开启扩展"));
        ui->extendGroupBox->hide();
    }
}

QString JtProtocolDialog::baseInfo()
{
    QString info;
    info.append(tr("SIM 卡号：%1\n").arg(ui->simLineEdit->text()));
    info.append(tr("终 端 ID：%1\n").arg(ui->devidLineEdit->text()));
    info.append(tr("终端型号：%1").arg(ui->devtypeLineEdit->text()));

    return info;
}

QByteArray JtProtocolDialog::encapsJTFrame(uint16_t command, const QByteArray &data)
{
    QByteArray generate;

    if (++flowseq == 0xFFFF) {
        flowseq = 1;
    }

    QByteArray sim = ui->simLineEdit->text().toLatin1();
    if (sim.size() >= 12) {
        sim = sim.right(12);
    } else {
        char unicode[] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
        sim.insert(0, unicode, 12 - sim.size());
    }

    generate.append(command >> 8);          // 命令
    generate.append(command >> 0);
    generate.append(data.size() >> 8);      // 数据的长度
    generate.append(data.size() >> 0);
    generate.append(QByteArray::fromHex(sim)); // SIM号码
    generate.append(flowseq >> 8);          // 序列计数
    generate.append(flowseq >> 0);
    generate.append(data);
    generate.append(calXorSumcheck(generate));

    return assembleByRules(generate, &sg_jtt_rules).toHex().toUpper();
}

QByteArray JtProtocolDialog::regiterFrame()
{
    QByteArray data;
    QByteArray manufacturerid = ui->manufactureridLineEdit->text().toLocal8Bit(); // 5
    QByteArray devtype = ui->devtypeLineEdit->text().toLocal8Bit(); // 20
    QByteArray devid = ui->devidLineEdit->text().toLocal8Bit(); // 7
    QByteArray platevin = ui->platevinLineEdit->text().toLocal8Bit(); // 17
    if (manufacturerid.size() < 5) {
        manufacturerid.append(5 - manufacturerid.size(), '\0');
    }
    if (devtype.size() < 20) {
        devtype.append(20 - devtype.size(), '\0');
    }
    if (devid.size() < 7) {
        devid.append(7 - devid.size(), '\0');
    }
    if (platevin.size() < 17) {
        platevin.append(17 - platevin.size(), '\0');
    }

    int code = ui->provinceComboBox->currentData().toInt();
    data.append(code >> 8); // 省域
    data.append(code >> 0);
    data.append(ui->cityLineEdit->text().toUShort() >> 8); // 市县域
    data.append(ui->cityLineEdit->text().toUShort() >> 0);
    data.append(manufacturerid);
    data.append(devtype);
    data.append(devid);
    data.append(ui->colourComboBox->currentIndex());
    data.append(platevin);

    if ("关闭扩展" == ui->extendButton->text()) {
        QByteArray companyid = ui->companyidComboBox->currentData().toString().toLocal8Bit(); // 8
        uint8_t digest = ui->digestComboBox->currentIndex();
        uint8_t encrypt = ui->encryptComboBox->currentIndex();
        QByteArray secretkey = QByteArray::fromHex(ui->secretkeyPlainTextEdit->toPlainText().toLatin1());
        QByteArray chipid = ui->chipidLineEdit->text().toLocal8Bit();
        QByteArray chipmakers = ui->chipmakersLineEdit->text().toLocal8Bit();
        QByteArray iccid = ui->iccidLineEdit->text().toLocal8Bit(); // 20
        if (companyid.size() < 8) {
            companyid.append(8 - companyid.size(), '\0');
        }
        if (iccid.size() < 20) {
            iccid.append(20 - iccid.size(), '\0');
        }

        data.append(companyid);
        data.append(digest);
        data.append(encrypt);
        data.append(secretkey.size());
        data.append(secretkey);
        data.append(chipid.size());
        data.append(chipid);
        data.append(chipmakers.size());
        data.append(chipmakers);
        data.append(iccid);
    }
    
    return encapsJTFrame(UP_REG, data);
}

QByteArray JtProtocolDialog::loginFrame()
{
    QByteArray data;
    data.append(ui->authcodeLineEdit->text());
    
    return encapsJTFrame(UP_AULOG, data);
}

QByteArray JtProtocolDialog::logoutFrame()
{
    QByteArray data;
    return encapsJTFrame(UP_REG_LOGOUT, data);
}

