#include "parsecommanddialog.h"
#include "ui_parsecommanddialog.h"

#include <QDebug>

#define INT16U_HIGH_LOW(high, low)   \
    (uint16_t)((uint8_t)(high) << 8 | (uint8_t)(low) << 0)
#define INT32U_BYTES(byte1, byte2, byte3, byte4)   \
    (uint32_t)((uint8_t)(byte1) << 24 | (uint8_t)(byte2) << 16 | \
    (uint8_t)(byte3) << 8 | (uint8_t)(byte4) << 0)

#define TOUPPER(val, fieldWidth)  \
    QString("%1").arg(val, fieldWidth, 16, QChar('0')).toUpper()

#define VALID_VAL(min, max, val) \
    QString("<font color='blue'>[%1,%2]</font> <font color='red'><b>%3</b></font>")\
    .arg(min).arg((double)max, 0, 'g', 10).arg((val < min || val > max)? "无效" : "")

#define CINDENT    tr("&nbsp;&nbsp;")
#define CTAB_T     tr("&nbsp;&nbsp;&nbsp;&nbsp;")
#define CTAB_2T    CTAB_T + CTAB_T
#define CTAB_3T    CTAB_T + CTAB_T + CTAB_T

static QString bcd2StrDateTime(const QByteArray &bcdDateTime)
{
    int i = 0;
    int year = bcdDateTime.at(i++);
    int month = bcdDateTime.at(i++);
    int day = bcdDateTime.at(i++);
    int hour = bcdDateTime.at(i++);
    int minute = bcdDateTime.at(i++);
    int second = bcdDateTime.at(i++);

    return QObject::tr("%1-%2-%3 %4:%5:%6").arg(year)
                .arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'))
                .arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0'))
                .arg(second, 2, 10, QChar('0'));
}

static QByteArray hexStr2ByteArray(const QString &hexs)
{
    QByteArray byteArray;
    
    for (int i = 0; i < hexs.length(); i += 2) {
        byteArray.append(hexs.mid(i, 2).toUShort(nullptr, 16));
    }

    return byteArray;
}

static uint8_t calXorSumcheck(const QByteArray &data)
{
    uint8_t result = 0;
    
    for (int i = 0; i < data.size(); ++i) {
        result ^= data.at(i);
    }

    return result;
}

ParseCommandDialog::ParseCommandDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParseCommandDialog)
{
    ui->setupUi(this);

    connect(ui->parseTextEdit, QTextEdit::textChanged, this, ParseCommandDialog::doTextChanged);
}

ParseCommandDialog::~ParseCommandDialog()
{
    delete ui;
}

void ParseCommandDialog::setProtocol(const QString &protocol, QObject *obj)
{
    m_obj = obj;
    m_protocol = protocol;
    ui->parseTextEdit->clear();
    ui->parsedTextBrowser->clear();
    setWindowTitle(m_protocol + "-解析");
}

// --------- 国六(HJ1239)终端排放协议 ----------//

QString ParseCommandDialog::g6HJ1239_SignatureParse(const QString &tabs, const QByteArray &signature)
{
    int i = 0;
    QString tmp;
    if (!signature.isEmpty()) {
        ushort rLen = (uint8_t)signature.at(i++);
        QString rValue = signature.mid(i, rLen).toHex().toUpper();
        i += rLen;
        ushort sLen = (uint8_t)signature.at(i++);
        QString sValue = signature.mid(i, sLen).toHex().toUpper();
        i += sLen;
        tmp.append(tabs + tr("签名信息：{<br>"));
        tmp.append(tabs + CTAB_T + tr("签名R值长度(0x%2)：%3<br>").arg(TOUPPER(rLen, 2)).arg(rLen));
        tmp.append(tabs + CTAB_T + tr("签名R值：0x%2<br>").arg(rValue));
        tmp.append(tabs + CTAB_T + tr("签名S值长度(0x%2)：%3<br>").arg(TOUPPER(sLen, 2)).arg(sLen));
        tmp.append(tabs + CTAB_T + tr("签名S值：0x%2<br>").arg(sValue));
        if (sLen > sValue.length()/2) {
            tmp.append(tabs + CTAB_T + tr("<<< 解析出错, 签名长度错误 >>><br>"));
            return tmp;
        }

        QString derR = tr("%1%2").arg(TOUPPER(rLen, 2)).arg(rValue);
        QString derS = tr("%1%2").arg(TOUPPER(sLen, 2)).arg(sValue);
        //qDebug() << "derR" << derR << "derS" << derS;
        tmp.append(tabs + CTAB_T + tr("DER格式(30+L1+02+L2+R+02+L3+S)：0x304402") + derR + "02" + derS + "<br>");
        tmp.append(tabs + tr("}<br>"));
    }

    return tmp;
}

QString ParseCommandDialog::g6HJ1239_RealTimeInfoParse(const QByteArray &data)
{
    int i = 0;
    QString tmp;
    if (i + 8 > data.size())
        return tmp;

    tmp.append(CTAB_T + tr("数据发送时间：%1<br>").arg(bcd2StrDateTime(data.mid(i, 6))));
    i += 6;
    uint seq = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
    i += 2;
    tmp.append(CTAB_T + tr("信息流水号(0x%1)：%2<br>").arg(TOUPPER(seq, 4)).arg(seq));
    ushort type = 0;
    bool isParseOk = true;
    for (int count = 0; isParseOk && (0x20 != type); ) {
        type = (uint8_t)data.at(i++);
        qDebug() << "type" << type;
        if ((i + 6) > data.size()) {
            break;
        }
        switch (type) {
        case 0x01: {
            tmp.append(CTAB_T + tr("信息编号(%1)<br>").arg(++count));
            tmp.append(CTAB_T + tr("车载诊断系统(OBD)信息(0x%1)：{<br>").arg(TOUPPER(type, 2)));

            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            ushort protocol = (uint8_t)data.at(i++);
            ushort milState = (uint8_t)data.at(i++);
            uint support = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint readiness = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            QByteArray vin = data.mid(i, 17);
            i += 17;
            QByteArray calNum = data.mid(i, 18);
            i += 18;
            QByteArray cvn = data.mid(i, 18);
            i += 18;
            QByteArray iupr = data.mid(i, 36);
            i += 36;
            ushort faultTotal = (uint8_t)data.at(i++);
            QList<ulong> fault;
            for (int j = 0; j < faultTotal; ++j, i += 4) {
                ulong val = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
                fault << val;
            }

            tmp.append(CTAB_2T + tr("信息采集时间：%1<br>").arg(dateTime));
            tmp.append(CTAB_2T + tr("OBD诊断协议(0x%1)：").arg(TOUPPER(protocol, 2)));
            switch (protocol) {
                case 0x00: tmp.append("ISO15765<br>"); break;
                case 0x01: tmp.append("ISO27145<br>"); break;
                case 0x02: tmp.append("SAEJ1939<br>"); break;
                default: tmp.append("无效<br>"); break;
            }
            tmp.append(CTAB_2T + tr("MIL状态(0x%1)：").arg(TOUPPER(milState, 2)));
            switch (milState) {
                case 0x00: tmp.append("未点亮<br>"); break;
                case 0x01: tmp.append("点亮<br>"); break;
                default: tmp.append("无效<br>"); break;
            }
            tmp.append(CTAB_2T + tr("诊断支持状态：0x%2<br>").arg(TOUPPER(support, 4)));
            tmp.append(CTAB_2T + tr("诊断就绪状态：0x%2<br>").arg(TOUPPER(readiness, 4)));
            tmp.append(CTAB_2T + tr("车辆识别代号(VIN)(%1)：%2<br>").arg(vin.size()).arg(QString(vin)));
            tmp.append(CTAB_2T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(vin.toHex().toUpper())));
            tmp.append(CTAB_2T + tr("软件标定识别号(%1)：%2<br>").arg(calNum.size()).arg(QString(calNum)));
            tmp.append(CTAB_2T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(calNum.toHex().toUpper())));
            tmp.append(CTAB_2T + tr("标定验证码(CVN)(%1)：%2<br>").arg(cvn.size()).arg(QString(cvn)));
            tmp.append(CTAB_2T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(cvn.toHex().toUpper())));
            tmp.append(CTAB_2T + tr("在用监测频率(IUPR)(%1)：0x%2<br>").arg(iupr.size()).arg(QString(iupr.toHex().toUpper())));
            tmp.append(CTAB_2T + tr("故障码总数(0x%1)：%2<br>").arg(TOUPPER(faultTotal, 2)).arg(faultTotal));
            if (fault.size() > 0) {
                tmp.append(CTAB_2T + tr("故障码信息列表：{<br>"));
                for (int j = 0; j < fault.size(); ++j) {
                    tmp.append(CTAB_3T + tr("故障码%1(0x%2)<br>").arg(j).arg(TOUPPER(fault.at(j), 8)));
                }
                tmp.append(CTAB_2T + tr("}<br>"));
            }
            break;
        }
        case 0x02: {
            tmp.append(CTAB_T + tr("信息编号(%1)<br>").arg(++count));
            tmp.append(CTAB_T + tr("颗粒捕集器(DPF)和/或催化还原(SCR)技术(0x%1)：{<br>").arg(TOUPPER(type, 2)));

            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint speed = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort pressure = (uint8_t)data.at(i++);
            ushort outTorque = (uint8_t)data.at(i++);
            ushort rubTorque = (uint8_t)data.at(i++);
            uint rpm = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint fuelFlow = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint scrUpNo = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint scrDnNo = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort reactant = (uint8_t)data.at(i++);
            uint airFlow  = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint scrInTemp = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint scrOutTemp = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint dpf = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort coolantTemp = (uint8_t)data.at(i++);
            ushort oilLevel = (uint8_t)data.at(i++);
            ushort posState = (uint8_t)data.at(i++);
            ulong longitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong latitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong mileage = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;

            tmp.append(CTAB_2T + tr("信息采集时间：%1<br>").arg(dateTime));
            tmp.append(CTAB_2T + tr("车速(0x%1)：%2km/h %3<br>").arg(TOUPPER(speed, 4)).arg(speed/256).arg(VALID_VAL(0, 250.996, speed *0.00390625)));
            tmp.append(CTAB_2T + tr("大气压力(0x%1)：%2kPa %3<br>").arg(TOUPPER(pressure, 2)).arg(pressure*0.5).arg(VALID_VAL(0, 125, pressure*0.5)));
            tmp.append(CTAB_2T + tr("发动机净输出扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(outTorque, 2)).arg(outTorque-125).arg(VALID_VAL(-125, 125, outTorque-125)));
            tmp.append(CTAB_2T + tr("摩擦扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(rubTorque, 2)).arg(rubTorque-125).arg(VALID_VAL(-125, 125, rubTorque-125)));
            tmp.append(CTAB_2T + tr("发动机转速(0x%1)：%2rpm %3<br>").arg(TOUPPER(rpm, 4)).arg(rpm*0.125).arg(VALID_VAL(0, 8031.875, rpm*0.125)));
            tmp.append(CTAB_2T + tr("发动机燃料流量(0x%1)：%2L/h %3<br>").arg(TOUPPER(fuelFlow, 4)).arg(fuelFlow*0.05).arg(VALID_VAL(0, 3212.75, fuelFlow*0.05)));
            tmp.append(CTAB_2T + tr("SCR上游NOx传感器输出值(0x%1)：%2ppm %3<br>").arg(TOUPPER(scrUpNo, 4)).arg(scrUpNo*0.05-200).arg(VALID_VAL(-200, 3012.75, scrUpNo*0.05-200)));
            tmp.append(CTAB_2T + tr("SCR下游NOx传感器输出值(0x%1)：%2ppm %3<br>").arg(TOUPPER(scrDnNo, 4)).arg(scrDnNo*0.05-200).arg(VALID_VAL(-200, 3012.75, scrDnNo*0.05-200)));
            tmp.append(CTAB_2T + tr("反应剂余量(0x%1)：%2% %3<br>").arg(TOUPPER(reactant, 2)).arg(reactant*0.4).arg(VALID_VAL(0, 100, reactant*0.4)));
            tmp.append(CTAB_2T + tr("进气量(0x%1)：%2kg/h %3<br>").arg(TOUPPER(airFlow, 4)).arg(airFlow*0.05).arg(VALID_VAL(0, 3212.75, airFlow*0.05)));
            tmp.append(CTAB_2T + tr("SCR入口温度(0x%1)：%2deg C %3<br>").arg(TOUPPER(scrInTemp, 4)).arg(scrInTemp*0.03125-273).arg(VALID_VAL(-273, 1734.96875, scrInTemp*0.03125-273)));
            tmp.append(CTAB_2T + tr("SCR出口温度(0x%1)：%2deg C %3<br>").arg(TOUPPER(scrOutTemp, 4)).arg(scrOutTemp*0.03125-273).arg(VALID_VAL(-273, 1734.96875, scrOutTemp*0.03125-273)));
            tmp.append(CTAB_2T + tr("DPF压差(0x%1)：%2kPa %3<br>").arg(TOUPPER(dpf, 4)).arg(dpf*0.1).arg(VALID_VAL(0, 6425.5, dpf*0.1)));
            tmp.append(CTAB_2T + tr("发动机冷却液温度(0x%1)：%2deg C %3<br>").arg(TOUPPER(coolantTemp, 2)).arg(coolantTemp-40).arg(VALID_VAL(-40, 210, coolantTemp-40)));
            tmp.append(CTAB_2T + tr("油箱液位(0x%1)：%2% %3<br>").arg(TOUPPER(oilLevel, 2)).arg(oilLevel*0.4).arg(VALID_VAL(0, 100, oilLevel*0.4)));
            tmp.append(CTAB_2T + tr("定位状态(0x%1)：(").arg(TOUPPER(posState, 2)));
            (posState & 0x01)? tmp.append("无效定位") : tmp.append("有效定位");
            (posState & 0x02)? tmp.append("|南纬") : tmp.append("|北纬");
            (posState & 0x04)? tmp.append("|西经)<br>") : tmp.append("|东经)<br>");
            tmp.append(CTAB_2T + tr("经度(0x%1)：%2° %3<br>").arg(TOUPPER(longitude, 8)).arg(longitude*0.000001).arg(VALID_VAL(0, 180.000000, longitude*0.000001)));
            tmp.append(CTAB_2T + tr("纬度(0x%1)：%2° %3<br>").arg(TOUPPER(latitude, 8)).arg(latitude*0.000001).arg(VALID_VAL(0, 90.000000, latitude*0.000001)));
            tmp.append(CTAB_2T + tr("累计里程(0x%1)：%2km %3<br>").arg(TOUPPER(mileage, 8)).arg(mileage*0.1).arg(VALID_VAL(0, 429496729.4, mileage*0.1)));
            break;
        }
        case 0x03: {
            tmp.append(CTAB_T + tr("信息编号(%1)<br>").arg(++count));
            tmp.append(CTAB_T + tr("三元催化器(TWC),无NOx传感器(0x%1)：{<br>").arg(TOUPPER(type, 2)));
            
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint speed = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort pressure = (uint8_t)data.at(i++);
            ushort outTorque = (uint8_t)data.at(i++);
            ushort rubTorque = (uint8_t)data.at(i++);
            uint rpm = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint fuelFlow = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint twcUpSensor = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort twcDnSensor = (uint8_t)data.at(i++);
            uint airFlow  = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort coolantTemp = (uint8_t)data.at(i++);
            uint twcTemp = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort posState = (uint8_t)data.at(i++);
            ulong longitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong latitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong mileage = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            
            tmp.append(CTAB_2T + tr("信息采集时间：%1<br>").arg(dateTime));
            tmp.append(CTAB_2T + tr("车速(0x%1)：%2km/h %3<br>").arg(TOUPPER(speed, 4)).arg(speed/256).arg(VALID_VAL(0, 250.996, speed *0.00390625)));
            tmp.append(CTAB_2T + tr("大气压力(0x%1)：%2kPa %3<br>").arg(TOUPPER(pressure, 2)).arg(pressure*0.5).arg(VALID_VAL(0, 125, pressure*0.5)));
            tmp.append(CTAB_2T + tr("发动机净输出扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(outTorque, 2)).arg(outTorque-125).arg(VALID_VAL(-125, 125, outTorque-125)));
            tmp.append(CTAB_2T + tr("摩擦扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(rubTorque, 2)).arg(rubTorque-125).arg(VALID_VAL(-125, 125, rubTorque-125)));
            tmp.append(CTAB_2T + tr("发动机转速(0x%1)：%2rpm %3<br>").arg(TOUPPER(rpm, 4)).arg(rpm*0.125).arg(VALID_VAL(0, 8031.875, rpm*0.125)));
            tmp.append(CTAB_2T + tr("发动机燃料流量(0x%1)：%2L/h %3<br>").arg(TOUPPER(fuelFlow, 4)).arg(fuelFlow*0.05).arg(VALID_VAL(0, 3212.75, fuelFlow*0.05)));
            tmp.append(CTAB_2T + tr("三元催化器上游氧传感器输出值(0x%1)：%2 %3<br>").arg(TOUPPER(twcUpSensor, 4)).arg(twcUpSensor*0.0000305).arg(VALID_VAL(0, 1.999, twcUpSensor*0.0000305)));
            tmp.append(CTAB_2T + tr("三元催化器下游氧传感器输出值(0x%1)：%2V %3<br>").arg(TOUPPER(twcDnSensor, 2)).arg(twcDnSensor*0.01).arg(VALID_VAL(0, 2.550, twcDnSensor*0.01)));
            tmp.append(CTAB_2T + tr("进气量(0x%1)：%2kg/h %3<br>").arg(TOUPPER(airFlow, 4)).arg(airFlow*0.05).arg(VALID_VAL(0, 3212.75, airFlow*0.05)));
            tmp.append(CTAB_2T + tr("发动机冷却液温度(0x%1)：%2deg C %3<br>").arg(TOUPPER(coolantTemp, 2)).arg(coolantTemp-40).arg(VALID_VAL(-40, 210, coolantTemp-40)));
            tmp.append(CTAB_2T + tr("三元催化器温度传感器(0x%1)：%2deg C %3<br>").arg(TOUPPER(twcTemp, 4)).arg(twcTemp*0.03125-273).arg(VALID_VAL(-273, 1734.96875, twcTemp*0.03125-273)));
            tmp.append(CTAB_2T + tr("定位状态(0x%1)：(").arg(TOUPPER(posState, 2)));
            (posState & 0x01)? tmp.append("无效定位") : tmp.append("有效定位");
            (posState & 0x02)? tmp.append("|南纬") : tmp.append("|北纬");
            (posState & 0x04)? tmp.append("|西经)<br>") : tmp.append("|东经)<br>");
            tmp.append(CTAB_2T + tr("经度(0x%1)：%2° %3<br>").arg(TOUPPER(longitude, 8)).arg(longitude*0.000001).arg(VALID_VAL(0, 180.000000, longitude*0.000001)));
            tmp.append(CTAB_2T + tr("纬度(0x%1)：%2° %3<br>").arg(TOUPPER(latitude, 8)).arg(latitude*0.000001).arg(VALID_VAL(0, 90.000000, latitude*0.000001)));
            tmp.append(CTAB_2T + tr("累计里程(0x%1)：%2km %3<br>").arg(TOUPPER(mileage, 8)).arg(mileage*0.1).arg(VALID_VAL(0, 429496729.4, mileage*0.1)));
            break;
        }
        case 0x04: {
            tmp.append(CTAB_T + tr("信息编号(%1)<br>").arg(++count));
            tmp.append(CTAB_T + tr("混动车附加(0x%1)：{<br>").arg(TOUPPER(type, 2)));
            
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint speed = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort motorSOC = (uint8_t)data.at(i++);
            uint voltage = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint circuit = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort batterySOC = (uint8_t)data.at(i++);
            
            tmp.append(CTAB_2T + tr("信息采集时间：%1<br>").arg(dateTime));
            tmp.append(CTAB_2T + tr("电机转速(0x%1)：%2rpm %3<br>").arg(TOUPPER(speed, 4)).arg(speed).arg(VALID_VAL(0, 65535, speed*1.0)));
            tmp.append(CTAB_2T + tr("电机负荷百分比(0x%1)：%2% %3<br>").arg(TOUPPER(motorSOC, 2)).arg(motorSOC).arg(VALID_VAL(0, 100, motorSOC*1.0)));
            tmp.append(CTAB_2T + tr("电池电压(0x%1)：%2V %3<br>").arg(TOUPPER(voltage, 4)).arg(voltage*0.1).arg(VALID_VAL(0, 6553.4, voltage*0.1)));
            tmp.append(CTAB_2T + tr("电池电流(0x%1)：%2A %3<br>").arg(TOUPPER(circuit, 4)).arg(circuit*0.1-1000).arg(VALID_VAL(-1000, 5553.4, circuit*0.1-1000)));
            tmp.append(CTAB_2T + tr("电池电量百分比(0x%1)：%2% %3<br>").arg(TOUPPER(batterySOC, 2)).arg(batterySOC).arg(VALID_VAL(0, 100, batterySOC*1.0)));
            break;
        }
        case 0x05: {
            tmp.append(CTAB_T + tr("信息编号(%1)<br>").arg(++count));
            tmp.append(CTAB_T + tr("三元催化器(TWC),有NOx传感器(0x%1)：{<br>").arg(TOUPPER(type, 2)));
            
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint speed = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort pressure = (uint8_t)data.at(i++);
            ushort outTorque = (uint8_t)data.at(i++);
            ushort rubTorque = (uint8_t)data.at(i++);
            uint rpm = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint fuelFlow = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint twcUpSensor = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort twcDnSensor = (uint8_t)data.at(i++);
            uint twcDnNo = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            uint airFlow  = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort coolantTemp = (uint8_t)data.at(i++);
            uint twcTemp = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort posState = (uint8_t)data.at(i++);
            ulong longitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong latitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong mileage = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            
            tmp.append(CTAB_2T + tr("信息采集时间：%1<br>").arg(dateTime));
            tmp.append(CTAB_2T + tr("车速(0x%1)：%2km/h %3<br>").arg(TOUPPER(speed, 4)).arg(speed/256).arg(VALID_VAL(0, 250.996, speed *0.00390625)));
            tmp.append(CTAB_2T + tr("大气压力(0x%1)：%2kPa %3<br>").arg(TOUPPER(pressure, 2)).arg(pressure*0.5).arg(VALID_VAL(0, 125, pressure*0.5)));
            tmp.append(CTAB_2T + tr("发动机净输出扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(outTorque, 2)).arg(outTorque-125).arg(VALID_VAL(-125, 125, outTorque-125)));
            tmp.append(CTAB_2T + tr("摩擦扭矩(0x%1)：%2% %3<br>").arg(TOUPPER(rubTorque, 2)).arg(rubTorque-125).arg(VALID_VAL(-125, 125, rubTorque-125)));
            tmp.append(CTAB_2T + tr("发动机转速(0x%1)：%2rpm %3<br>").arg(TOUPPER(rpm, 4)).arg(rpm*0.125).arg(VALID_VAL(0, 8031.875, rpm*0.125)));
            tmp.append(CTAB_2T + tr("发动机燃料流量(0x%1)：%2L/h %3<br>").arg(TOUPPER(fuelFlow, 4)).arg(fuelFlow*0.05).arg(VALID_VAL(0, 3212.75, fuelFlow*0.05)));
            tmp.append(CTAB_2T + tr("三元催化器上游氧传感器输出值(0x%1)：%2 %3<br>").arg(TOUPPER(twcUpSensor, 4)).arg(twcUpSensor*0.0000305).arg(VALID_VAL(0, 1.999, twcUpSensor*0.0000305)));
            tmp.append(CTAB_2T + tr("三元催化器下游氧传感器输出值(0x%1)：%2V %3<br>").arg(TOUPPER(twcDnSensor, 2)).arg(twcDnSensor*0.01).arg(VALID_VAL(0, 2.550, twcDnSensor*0.01)));
            tmp.append(CTAB_2T + tr("三元催化器下游NOx传感器输出值(0x%1)：%2ppm %3<br>").arg(TOUPPER(twcDnNo, 4)).arg(twcDnNo*0.05-200).arg(VALID_VAL(-200, 3012.75, twcDnNo*0.05-200)));
            tmp.append(CTAB_2T + tr("进气量(0x%1)：%2kg/h %3<br>").arg(TOUPPER(airFlow, 4)).arg(airFlow*0.05).arg(VALID_VAL(0, 3212.75, airFlow*0.05)));
            tmp.append(CTAB_2T + tr("发动机冷却液温度(0x%1)：%2deg C %3<br>").arg(TOUPPER(coolantTemp, 2)).arg(coolantTemp-40).arg(VALID_VAL(-40, 210, coolantTemp-40)));
            tmp.append(CTAB_2T + tr("三元催化器温度传感器(0x%1)：%2deg C %3<br>").arg(TOUPPER(twcTemp, 4)).arg(twcTemp*0.03125-273).arg(VALID_VAL(-273, 1734.96875, twcTemp*0.03125-273)));
            tmp.append(CTAB_2T + tr("定位状态(0x%1)：(").arg(TOUPPER(posState, 2)));
            (posState & 0x01)? tmp.append("无效定位") : tmp.append("有效定位");
            (posState & 0x02)? tmp.append("|南纬") : tmp.append("|北纬");
            (posState & 0x04)? tmp.append("|西经)<br>") : tmp.append("|东经)<br>");
            tmp.append(CTAB_2T + tr("经度(0x%1)：%2° %3<br>").arg(TOUPPER(longitude, 8)).arg(longitude*0.000001).arg(VALID_VAL(0, 180.000000, longitude*0.000001)));
            tmp.append(CTAB_2T + tr("纬度(0x%1)：%2° %3<br>").arg(TOUPPER(latitude, 8)).arg(latitude*0.000001).arg(VALID_VAL(0, 90.000000, latitude*0.000001)));
            tmp.append(CTAB_2T + tr("累计里程(0x%1)：%2km %3<br>").arg(TOUPPER(mileage, 8)).arg(mileage*0.1).arg(VALID_VAL(0, 429496729.4, mileage*0.1)));
            break;
        }
        default:
            isParseOk = (0x20 == type);
            if (!isParseOk) {
                tmp.append(CTAB_T + tr("<<< 解析出错, 未知类型：0x%1 >>><br>").arg(TOUPPER(type, 2)));
            }
            continue;
        }
        tmp.append(CTAB_T + tr("}<br>"));
    }

    if (isParseOk) {
        QByteArray signature = data.mid(i - 1);
        tmp.append(g6HJ1239_SignatureParse(CTAB_T, signature));
    }
    
    return tmp;
}

QString ParseCommandDialog::protocolParseG6HJ1239(QString &parse)
{
    QString parsed;
    parsed.insert(0, tr("*************** %1报文 ***************<br><br>").arg(m_protocol));

    QString tmp = parse.replace(" ", "");
    QByteArray sc = QByteArray::fromHex(tmp.toLatin1());
    uint8_t val1 = calXorSumcheck(sc.left(sc.size() - 1));
    uint8_t val2 = (uint8_t)(sc.right(1).at(0));
    if (val1 != val2) {
        parsed.append(tr("校验码检验失败：0x%1 0x%2<br><br>").arg(TOUPPER(val1, 2)).arg(TOUPPER(val2, 2)));
        return parsed;
    }
    
    uint8_t cmd = tmp.mid(4, 2).toUShort(nullptr, 16);
    QByteArray vin = QByteArray::fromHex(tmp.mid(6, 17 * 2).toLatin1());
    uint8_t versionNumber = tmp.mid(40, 2).toUShort(nullptr, 16);
    uint8_t encryptionMode = tmp.mid(42, 2).toUShort(nullptr, 16);
    uint16_t dataLen = tmp.mid(44, 4).toUInt(nullptr, 16);

    parsed.append(tr("命令单元(0x%1)：").arg(tmp.mid(4, 2)));
    switch (cmd) {
        case 0x01: parsed.append("车辆登入<br>"); break;
        case 0x02: parsed.append("实时信息传输<br>"); break;
        case 0x03: parsed.append("补传信息传输<br>"); break;
        case 0x04: parsed.append("车辆登出<br>"); break;
        case 0x05: parsed.append("终端校时<br>"); break;
        case 0x06: parsed.append("车辆拆除报警信息<br>"); break;
        case 0x07: parsed.append("激活信息<br>"); break;
        case 0x08: parsed.append("激活信息应答<br>"); break;
        default: parsed.append("未知命令<br>"); break;
    }
    parsed.append(tr("车辆识别代号(VIN)(%1)：%2<br>").arg(vin.size()).arg(QString(vin)));
    parsed.append(CINDENT + tr("原始数据：0x%1<br>").arg(tmp.mid(6, 17 * 2)));
    parsed.append(tr("终端软件版本号/流水号(0x%1)：%2<br>").arg(tmp.mid(40, 2)).arg(versionNumber));
    parsed.append(tr("数据加密方式(0x%1)：").arg(tmp.mid(42, 2)));
    switch (encryptionMode) {
        case 0x01: parsed.append("数据不加密<br>"); break;
        case 0x02: parsed.append("SM2算法加密<br>"); break;
        case 0x03: parsed.append("SM4位算法加密<br>"); break;
        case 0x04: parsed.append("RSA算法加密<br>"); break;
        case 0x05: parsed.append("AES128算法加密<br>"); break;
        case 0xFE: parsed.append("异常<br>"); break;
        case 0xFF: parsed.append("无效<br>"); break;
        default: break;
    }

    parsed.append(tr("数据单元长度(0x%1)：%2字节<br>").arg(tmp.mid(44, 4)).arg(dataLen));
    parsed.append("数据单元：{<br>");
    QByteArray data = QByteArray::fromHex(tmp.mid(48).toLatin1());
    QString duc;
    qDebug() << "cmd" << cmd;
    switch (cmd) {
        case 0x01: {
            int i = 0;
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint seq = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            QByteArray iccid = data.mid(i, dataLen-i);
            
            duc.append(CTAB_T + tr("登入时间：%1<br>").arg(dateTime));
            duc.append(CTAB_T + tr("登入流水号(0x%1)：%2<br>").arg(TOUPPER(seq, 4)).arg(seq));
            duc.append(CTAB_T + tr("SIM卡号(%1)：%2<br>").arg(iccid.size()).arg(QString(iccid)));
            duc.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(iccid.toHex().toUpper())));
            break;
        }
        case 0x02:
        case 0x03:
            duc = g6HJ1239_RealTimeInfoParse(data);
            break;
        case 0x04: {
            int i = 0;
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint seq = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            
            duc.append(CTAB_T + tr("登出时间：%1<br>").arg(dateTime));
            duc.append(CTAB_T + tr("登出流水号(0x%1)：%2<br>").arg(TOUPPER(seq, 4)).arg(seq));
            break;
        }
        case 0x05: {
            // 数据单元内容为空
            break;
        }
        case 0x06: {
            int i = 0;
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            uint seq = INT16U_HIGH_LOW(data.at(i), data.at(i + 1));
            i += 2;
            ushort posState = (uint8_t)data.at(i++);
            ulong longitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            ulong latitude = INT32U_BYTES(data.at(i), data.at(i + 1), data.at(i + 2), data.at(i + 3));
            i += 4;
            
            duc.append(CTAB_T + tr("数据采集时间：%1<br>").arg(dateTime));
            duc.append(CTAB_T + tr("流水号(0x%1)：%2<br>").arg(TOUPPER(seq, 4)).arg(seq));
            duc.append(CTAB_T + tr("定位状态(0x%1)：(").arg(TOUPPER(posState, 2)));
            (posState & 0x01)? duc.append("无效定位") : duc.append("有效定位");
            (posState & 0x02)? duc.append("|南纬") : duc.append("|北纬");
            (posState & 0x04)? duc.append("|西经)<br>") : duc.append("|东经)<br>");
            duc.append(CTAB_T + tr("经度(0x%1)：%2°<br>").arg(TOUPPER(longitude, 8)).arg(longitude*0.000001));
            duc.append(CTAB_T + tr("纬度(0x%1)：%2°<br>").arg(TOUPPER(latitude, 8)).arg(latitude*0.000001));
            break;
        }
        case 0x07: {
            int i = 0;
            QString dateTime = bcd2StrDateTime(data.mid(i, 6));
            i += 6;
            QByteArray chipsId = data.mid(i, 16);
            i += 16;
            QString publicKey = data.mid(i, 64).toHex().toUpper();
            i += 64;
            QByteArray vin = data.mid(i, 17);
            i += 17;
            
            duc.append(CTAB_T + tr("数据采集时间：%1<br>").arg(dateTime));
            duc.append(CTAB_T + tr("芯片ID(%1)：%2<br>").arg(chipsId.size()).arg(QString(chipsId)));
            duc.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(chipsId.toHex().toUpper())));
            duc.append(CTAB_T + tr("公钥内容：{<br>"));
            duc.append(CTAB_2T + tr("公钥 X(%1)：0x%2<br>").arg(publicKey.length()/4).arg(publicKey.mid(0, publicKey.length()/2)));
            duc.append(CTAB_2T + tr("公钥 Y(%1)：0x%2<br>").arg(publicKey.length()/4).arg(publicKey.mid(publicKey.length()/2, publicKey.length())));
            duc.append(CTAB_T + tr("}<br>"));
            duc.append(CTAB_T + tr("车辆识别代号(VIN)(%1)：%2<br>").arg(vin.size()).arg(QString(vin)));
            duc.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(vin.toHex().toUpper())));
            duc.append(g6HJ1239_SignatureParse(CTAB_T, data.mid(i)));
            break;
        }
        case 0x08: {
            int i = 0;
            ushort state = (uint8_t)data.at(i++);
            ushort info = (uint8_t)data.at(i++);
            
            duc.append(CTAB_T + tr("状态码(0x%1)：").arg(TOUPPER(state, 2)));
            switch (state) {
                case 0x01: duc.append("激活成功<br>"); break;
                case 0x02: duc.append("激活失败<br>"); break;
                default: duc.append("未知错误<br>"); break;
            }
            duc.append(CTAB_T + tr("信息(0x%1)：").arg(TOUPPER(info, 2)));
            switch (info) {
                case 0x00: duc.append("激活成功<br>"); break;
                case 0x01: duc.append("芯片已激活<br>"); break;
                case 0x02: duc.append("VIN错误<br>"); break;
                default: duc.append("未知错误<br>"); break;
            }
            break;
        }
        default: break;
    }
    parsed.append(duc);
    parsed.append("}<br>");
    parsed.append(tr("校验码：0x%1<br>").arg(tmp.mid(48 + dataLen * 2, 2)));
    
    return parsed;
}

// --------- 宁德时代 RDB解析协议命令 ----------//
QString ParseCommandDialog::NingDeShiDaiRDB_DataParse(uint8_t aid, uint8_t mid, 
                    const QByteArray &data, QString &dispatch)
{
    QString tmp;
    switch (aid) {
        case 0x81: // 数据上报
            switch (mid) {
                case 0x01: {
                    int i = 0;
                    ushort  count = data.at(i++);
                    ushort  flag1 = data.at(i++);
                    ulong saved1 = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ulong  msgid1 = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    QByteArray msg1 = data.mid(i, 8);
                    i += 8;
                    ushort  flag2 = data.at(i++);
                    ulong saved2 = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ulong  msgid2 = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    QByteArray msg2 = data.mid(i, 8);
                    i += 8;
                    
                    dispatch.append("BMU 数据上报");
                    tmp.append(CTAB_T + tr("PACKET-COUNT(0x%1)：%2<br>").arg(TOUPPER(count, 2)).arg(count));
                    tmp.append(CTAB_T + tr("Time Flag(0x%1)：%2<br>").arg(TOUPPER(flag1, 2)).arg(flag1));
                    tmp.append(CTAB_T + tr("BMU Msg Saved Time(0x%1)：%2<br>").arg(TOUPPER(saved1, 8)).arg(saved1));
                    tmp.append(CTAB_T + tr("BMU_MsgID(0x%1)：%2<br>").arg(TOUPPER(msgid1, 8)).arg(msgid1));
                    tmp.append(CTAB_T + tr("BMU_Msg：0x%1<br>").arg(QString(msg1.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("Time Flag(0x%1)：%2<br>").arg(TOUPPER(flag2, 2)).arg(flag2));
                    tmp.append(CTAB_T + tr("BMU Msg Saved Time(0x%1)：%2<br>").arg(TOUPPER(saved2, 8)).arg(saved2));
                    tmp.append(CTAB_T + tr("BMU_MsgID：0x%1<br>").arg(TOUPPER(msgid2, 8)));
                    tmp.append(CTAB_T + tr("BMU_Msg：0x%1<br>").arg(QString(msg2.toHex().toUpper())));
                    break;
                }
                case 0x02: {
                    int i = 0;
                    QByteArray rdbsn = data.mid(i, 32);
                    i += 32;
                    QByteArray rdbhw = data.mid(i, 16);
                    i += 16;
                    QByteArray rdbsoft = data.mid(i, 16);
                    i += 16;
                    QByteArray rdbimei = data.mid(i, 16);
                    i += 16;
                    QByteArray rdbiccid = data.mid(i, 20);
                    i += 20;
                    QByteArray rdbphone = data.mid(i, 16);
                    i += 16;
                    QByteArray rdbck = data.mid(i, 24);
                    i += 24;
                    QByteArray rdbmodem = data.mid(i, 24);
                    i += 24;
                    
                    dispatch.append("RDB 信息上报");
                    tmp.append(CTAB_T + tr("RDB-SN(%1)：%2<br>").arg(rdbsn.size()).arg(QString(rdbsn)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbsn.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("RDB-HWVERSION(%1)：%2<br>").arg(rdbhw.size()).arg(QString(rdbhw)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbhw.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("RDB-SOFTVERSION(%1)：%2<br>").arg(rdbsoft.size()).arg(QString(rdbsoft)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbsoft.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("IMEI(%1)：%2<br>").arg(rdbimei.size()).arg(QString(rdbimei)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbimei.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("ICCID(%1)：%2<br>").arg(rdbiccid.size()).arg(QString(rdbiccid)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbiccid.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("PHONENUM(%1)：%2<br>").arg(rdbphone.size()).arg(QString(rdbphone)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbphone.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("CHECKSUM(%1)：0x%2<br>").arg(rdbck.size()).arg(QString(rdbck.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("MODEMVERSION(%1)：%2<br>").arg(rdbmodem.size()).arg(QString(rdbmodem)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(rdbmodem.toHex().toUpper())));
                    break;
                }
                case 0x03: {
                    int i = 0;
                    ushort  work = data.at(i++);
                    ushort  state = data.at(i++);
                    ushort  progress = data.at(i++);
                    ushort  rssi = data.at(i++);
                    ushort  reserve = data.at(i++);
                    ulong flashinfo = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    QByteArray checksum = data.mid(i, 24);
                    QString workstr;
                    switch (work) {
                        case 0x00: workstr.append("正常"); break;
                        case 0x01: workstr.append("CAN bus off"); break;
                        case 0x02: workstr.append("CAN初始化失败"); break;
                        default: break;
                    }
                    QString statestr;
                    switch (state) {
                        case 0x00: statestr.append("默认值"); break;
                        case 0x01: statestr.append("传输请求"); break;
                        case 0x02: statestr.append("传输失败"); break;
                        case 0x03: statestr.append("正在传输"); break;
                        case 0x04: statestr.append("传输成功"); break;
                        default: break;
                    }
                    
                    dispatch.append("RDB 工作状态上报");
                    tmp.append(CTAB_T + tr("CAN 工作状态(0x%1)：%2<br>").arg(TOUPPER(work, 2)).arg(workstr));
                    tmp.append(CTAB_T + tr("BMU CAN 传输状态(0x%1)：%2<br>").arg(TOUPPER(state, 2)).arg(statestr));
                    tmp.append(CTAB_T + tr("BMU CAN 传输进度(0x%1)：%2%<br>").arg(TOUPPER(progress, 2)).arg(progress));
                    tmp.append(CTAB_T + tr("GSM 信号值(0x%1)：%2<br>").arg(TOUPPER(rssi, 2)).arg(rssi));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve, 2)).arg(reserve));
                    tmp.append(CTAB_T + tr("FLASH-INFO(0x%1)：%2<br>").arg(TOUPPER(flashinfo, 2)).arg(flashinfo));
                    tmp.append(CTAB_T + tr("配置文件摘要(%1)：0x%2<br>").arg(checksum.size()).arg(QString(checksum.toHex().toUpper())));
                    break;
                }
                case 0x04: {
                    int i = 0;
                    ushort  count = data.at(i++);
                    QByteArray bmuiversion = data.mid(i, 64);
                    i += 64;
                    QByteArray bmuinfo = data.mid(i, 32);
                    i += 32;
                    
                    dispatch.append("BMU 基本信息上报");
                    tmp.append(CTAB_T + tr("电箱个数(0x%1)：%2<br>").arg(TOUPPER(count, 2)).arg(count));
                    tmp.append(CTAB_T + tr("BMIU 版本号(%1)：%2<br>").arg(bmuiversion.size()).arg(QString(bmuiversion)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(bmuiversion.toHex().toUpper())));
                    tmp.append(CTAB_T + tr("BMU 兼容性信息(%1)：%2<br>").arg(bmuinfo.size()).arg(QString(bmuinfo)));
                    tmp.append(CTAB_T + CINDENT + tr("原始数据：0x%1<br>").arg(QString(bmuinfo.toHex().toUpper())));
                    for (int n = 0; n < count; ++n) {
                        ulong sncanid = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                        i += 4;
                        QByteArray sncode = data.mid(i, 8);
                        i += 8;

                        tmp.append(CTAB_T + tr("电箱 SN：CAN-ID：0x%1<br>").arg(TOUPPER(sncanid, 8)));
                        tmp.append(CTAB_T + tr("电箱 SN 码：0x%1<br>").arg(QString(sncode.toHex().toUpper())));
                    }
                    break;
                }
                case 0x05: {
                    int i = 0;
                    ushort  status = data.at(i++);
                    ulong longitude = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ulong latitude = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ulong utcvalue = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ushort  lonsuffix = data.at(i++);
                    ushort  lasuffix = data.at(i++);
                    ushort  coordinate = data.at(i++);
                    
                    dispatch.append("RDB 基站地理位置上报");
                    tmp.append(CTAB_T + tr("定位状态(0x%1)：%2<br>").arg(TOUPPER(status, 2)).arg(status? "无效" : "有效"));
                    tmp.append(CTAB_T + tr("经度(0x%1)：%2<br>").arg(TOUPPER(longitude, 8)).arg(longitude*0.000001));
                    tmp.append(CTAB_T + tr("纬度(0x%1)：%2<br>").arg(TOUPPER(latitude, 8)).arg(latitude*0.000001));
                    tmp.append(CTAB_T + tr("UTC时间(0x%1)：%2<br>").arg(TOUPPER(utcvalue, 8)).arg(utcvalue));
                    tmp.append(CTAB_T + tr("LonSuffix(0x%1)：%2<br>").arg(TOUPPER(lonsuffix, 2)).arg(lonsuffix? "西经" : "东经"));
                    tmp.append(CTAB_T + tr("LaSuffix(0x%1)：%2<br>").arg(TOUPPER(lasuffix, 2)).arg(lasuffix? "北纬" : "南纬"));
                    tmp.append(CTAB_T + tr("坐标系统(0x%1)：%2<br>").arg(TOUPPER(coordinate, 2)).arg(coordinate));
                    break;
                }
                case 0x10: {
                    int i = 0;
                    ushort  count = data.at(i++);
                    
                    dispatch.append("BMU7.0 数据上报");
                    tmp.append(CTAB_T + tr("PACKET-COUNT(0x%1)：%2<br>").arg(TOUPPER(count, 2)).arg(count));
                    for (int n = 0; n < count; ++n) {
                        ushort  flag = data.at(i++);
                        ushort  type = data.at(i++);
                        ulong life = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                        i += 4;
                        uint saved = INT16U_HIGH_LOW(data.at(i), data.at(i+1));
                        i += 2;
                        ulong  msgid = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                        i += 4;
                        QByteArray msg = data.mid(i, 8);
                        i += 8;
                        QString typestr;
                        switch (type) {
                            case 0: typestr.append("概要数据"); break;
                            case 1: typestr.append("详细数据"); break;
                            case 2: typestr.append("扩展数据"); break;
                            case 3: typestr.append("调试数据"); break;
                            default: typestr.append("未知数据"); break;
                        }

                        tmp.append(CTAB_T + tr("TIME FLAG %1(0x%2)：%3<br>").arg(n).arg(TOUPPER(flag, 2)).arg(flag));
                        tmp.append(CTAB_T + tr("BMU MSG Type(0x%1)：%2<br>").arg(type).arg(typestr));
                        tmp.append(CTAB_T + tr("BMU MSG Life Time(0x%1)：%2<br>").arg(TOUPPER(life, 8)).arg(life));
                        tmp.append(CTAB_T + tr("BMU MSG Saved Time(0x%1)：%2<br>").arg(TOUPPER(saved, 4)).arg(saved));
                        tmp.append(CTAB_T + tr("BMU MSG ID：0x%1<br>").arg(TOUPPER(msgid, 8)));
                        tmp.append(CTAB_T + tr("BMU MSG DATA：0x%1<br><br>").arg(QString(msg.toHex().toUpper())));
                    }
                    break;
                }
                case 0x20: {
                    int i = 0;
                    ushort  count = data.at(i++);
                    
                    dispatch.append("BMU6.0 数据上报");
                    tmp.append(CTAB_T + tr("PACKET-COUNT(0x%1)：%2<br>").arg(TOUPPER(count, 2)).arg(count));
                    for (int n = 0; n < count; ++n) {
                        ushort  flag = data.at(i++);
                        ushort  type = data.at(i++);
                        ulong life = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                        i += 4;
                        uint saved = INT16U_HIGH_LOW(data.at(i), data.at(i+1));
                        i += 2;
                        ulong  msgid = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                        i += 4;
                        QByteArray msg = data.mid(i, 8);
                        i += 8;
                        QString typestr;
                        switch (type) {
                            case 0: typestr.append("概要数据"); break;
                            case 1: typestr.append("详细数据"); break;
                            case 2: typestr.append("扩展数据"); break;
                            case 3: typestr.append("调试数据"); break;
                            default: typestr.append("未知数据"); break;
                        }

                        tmp.append(CTAB_T + tr("TIME FLAG %1(0x%2)：%3<br>").arg(n).arg(TOUPPER(flag, 2)).arg(flag));
                        tmp.append(CTAB_T + tr("BMU MSG Type(0x%1)：%2<br>").arg(type).arg(typestr));
                        tmp.append(CTAB_T + tr("BMU MSG Life Time(0x%1)：%2<br>").arg(TOUPPER(life, 8)).arg(life));
                        tmp.append(CTAB_T + tr("BMU MSG Saved Time(0x%1)：%2<br>").arg(TOUPPER(saved, 4)).arg(saved));
                        tmp.append(CTAB_T + tr("BMU MSG ID：0x%1<br>").arg(TOUPPER(msgid, 8)));
                        tmp.append(CTAB_T + tr("BMU MSG DATA：0x%1<br><br>").arg(QString(msg.toHex().toUpper())));
                    }
                    break;
                }
                default: break;
            }
            break;
        case 0x82: // 告警数据上报
            switch (mid) {
                case 0x01: {
                    int i = 0;
                    ulong utctime = INT32U_BYTES(data.at(i), data.at(i+1), data.at(i+2), data.at(i+3));
                    i += 4;
                    ushort  flag = data.at(i++);
                    ushort  can0link = data.at(i++);
                    ushort  can0protocol = data.at(i++);
                    ushort  reserve1 = data.at(i++);
                    ushort  reserve2 = data.at(i++);
                    ushort  reserve3 = data.at(i++);
                    ushort  reserve4 = data.at(i++);
                    ushort  gsmstate = data.at(i++);
                    ushort  flashstate = data.at(i++);
                    ushort  reserve5 = data.at(i++);
                    ushort  reserve6 = data.at(i++);
                    QString can0linkstr;
                    switch (can0link) {
                        case 0: can0linkstr.append("正常"); break;
                        case 1: can0linkstr.append("BUS-OFF"); break;
                        case 2: can0linkstr.append("初始化失败"); break;
                        default: can0linkstr.append("未知"); break;
                    }
                    QString can0protocolstr;
                    can0protocolstr.append((can0protocol & 0x01)? "BMU不在线|" : "BMU在线|");
                    can0protocolstr.append((can0protocol & 0x02)? "概要数据采集不完整" : "概要数据采集正常");
                    QString gsmstatestr;
                    gsmstatestr.append(gsmstate? "故障" : "正常");
                    QString flashstatestr;
                    flashstatestr.append(flashstate? "故障" : "正常");
                    
                    dispatch.append("报警数据1上报");
                    tmp.append(CTAB_T + tr("UTC时间(0x%1)：%2<br>").arg(TOUPPER(utctime, 8)).arg(utctime));
                    tmp.append(CTAB_T + tr("时间有效标识(0x%1)：%2<br>").arg(TOUPPER(flag, 2)).arg(flag));
                    tmp.append(CTAB_T + tr("CAN0-链路状态(0x%1)：%2<br>").arg(TOUPPER(can0link, 2)).arg(can0linkstr));
                    tmp.append(CTAB_T + tr("CAN0-协议状态(0x%1)：%2<br>").arg(TOUPPER(can0protocol, 2)).arg(can0protocolstr));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve1, 2)).arg(reserve1));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve2, 2)).arg(reserve2));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve3, 2)).arg(reserve3));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve4, 2)).arg(reserve4));
                    tmp.append(CTAB_T + tr("GSM状态(0x%1)：%2<br>").arg(TOUPPER(gsmstate, 2)).arg(gsmstatestr));
                    tmp.append(CTAB_T + tr("FLASH状态(0x%1)：%2<br>").arg(TOUPPER(flashstate, 2)).arg(flashstatestr));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve5, 2)).arg(reserve5));
                    tmp.append(CTAB_T + tr("保留(0x%1)：%2<br>").arg(TOUPPER(reserve6, 2)).arg(reserve6));
                    break;
                }
                case 0x02: {
                    int i = 0;
                    dispatch.append("报警数据2上报");
                    break;
                }
                default: break;
            }
            break;
        case 0x41: // 配置文件同步
            switch (mid) {
                case 0x01: {
                    dispatch.append("请求配置文件数据");
                    break;
                }
                default: break;
            }
            break;
        case 0x42: // RDB 升级
            switch (mid) {
                case 0x01: {
                    dispatch.append("请求下发固件下载地址");
                    break;
                }
                case 0x02: {
                    dispatch.append("上报下载固件开始");
                    break;
                }
                case 0x03: {
                    dispatch.append("上报数据下载结果检测");
                    break;
                }
                case 0x04: {
                    dispatch.append("上报开始升级");
                    break;
                }
                case 0x05: {
                    dispatch.append("上报升级结果");
                    break;
                }
                case 0x10: {
                    dispatch.append("取消升级");
                    break;
                }
                default: break;
            }
            break;
        case 0x43: // BMU 升级流程
            switch (mid) {
                case 0x01: {
                    dispatch.append("请求升级 BMU");
                    break;
                }
                case 0x02: {
                    dispatch.append("上报开始下载");
                    break;
                }
                case 0x03: {
                    dispatch.append("上报下载结果");
                    break;
                }
                case 0x04: {
                    dispatch.append("上报开始传输");
                    break;
                }
                case 0x05: {
                    dispatch.append("上报传输结果");
                    break;
                }
                case 0x06: {
                    dispatch.append("上报升级结果");
                    break;
                }
                case 0x07: {
                    dispatch.append("传输日志上报");
                    break;
                }
                case 0x10: {
                    dispatch.append("取消升级");
                    break;
                }
                default: break;
            }
            break;
        case 0x44: // 时间同步
            switch (mid) {
                case 0x01: {
                    dispatch.append("请求时间同步");
                    break;
                }
                default: break;
            }
            break;
        case 0x45: // 设备重启
            switch (mid) {
                case 0x01: {
                    dispatch.append("请求重启");
                    break;
                }
                case 0x02: {
                    dispatch.append("上报重启完成");
                    break;
                }
                default: break;
            }
            break;
        case 0xEF: // 参数配置/读取
            switch (mid) {
                case 0x01: {
                    dispatch.append("GSM 电话号码请求");
                    break;
                }
                case 0x02: {
                    dispatch.append("GSM RSSI请求");
                    break;
                }
                case 0x03: {
                    dispatch.append("GSM IMSI请求");
                    break;
                }
                case 0x04: {
                    dispatch.append("GSM IMEI请求");
                    break;
                }
                case 0x05: {
                    dispatch.append("GSM ICCID请求");
                    break;
                }
                case 0x31: {
                    dispatch.append("清除存储器数据请求");
                    break;
                }
                case 0x45: {
                    dispatch.append("CAN 读取 BMU 版本匹配信息请");
                    break;
                }
                default: break;
            }
            break;
        default: dispatch.append("未知调度数据"); break;
    }
    
    return tmp;
}


QString ParseCommandDialog::protocolParseNingDeShiDaiRDB(QString &parse)
{
    QString parsed;
    parsed.insert(0, tr("*************** %1报文 ***************<br><br>").arg(m_protocol));

    // BB CD FF 1F 10 00 01 81 20 00 4E 03 01 00 D2 DB 3F 80 3F 83 12 34 56 78 88 99 AA BB CC DD EE FF 01 D2 DB 3F 80 3F 83 12 34 56 78 88 99 AA BB CC DD EE FF 02 D2 DB 3F 80 3F 83 12 34 56 78 88 99 AA BB CC DD EE FF 03 D2 DB 3F 80 3F 83 12 34 56 78 88 99 AA BB CC DD EE FF
    QString tmp = parse.replace(" ", "");
    
    uint8_t aid = tmp.mid(14, 2).toUShort(nullptr, 16);
    uint8_t mid = tmp.mid(16, 2).toUShort(nullptr, 16);
    uint16_t datalen = tmp.mid(18, 4).toUShort(nullptr, 16);
    QByteArray data = QByteArray::fromHex(tmp.mid(22).toLatin1());
    QString dispatch;
    QString rdb = NingDeShiDaiRDB_DataParse(aid, mid, data, dispatch);

    parsed.append(tr("消息起始标识(SOF)：0x%1<br>").arg(tmp.mid(0, 4)));
    parsed.append(tr("设备识别码(SN)：0x%1<br>").arg(tmp.mid(4, 8)));
    parsed.append(tr("服务版本号：0x%1<br>").arg(tmp.mid(12, 2)));
    parsed.append(tr("调度数据(AID:0x%1 MID:0x%2)：%3<br>").arg(tmp.mid(14, 2), tmp.mid(16, 2), dispatch));
    if (datalen > 0) {
        parsed.append(tr("数据长度(0x%1)：%2<br>").arg(tmp.mid(18, 4)).arg(datalen));
        parsed.append(tr("数据：{<br>"));
        parsed.append(rdb);
        parsed.append(tr("}<br>"));
    }
    
    return parsed;
}

// --------- 交通部第二版解析协议命令 ----------//

QString ParseCommandDialog::jiaoTong2_parsedClientData(       uint16_t command, const QString &data)
{
    QString parsed;
    switch (command) {
        case UP_REG: {
            QString manufacturerid = hexStr2ByteArray(data.mid(8, 10));
            QString devtype = hexStr2ByteArray(data.mid(18, 40));
            QString devid = hexStr2ByteArray(data.mid(58, 14));
            QString platevin = hexStr2ByteArray(data.mid(74, 34));
            QString companyid = hexStr2ByteArray(data.mid(108, 16));
            uint8_t keylen = data.mid(128, 2).toInt(nullptr, 16);
            uint8_t province = data.mid(0, 4).toInt(nullptr, 16);
            parsed.append(CTAB_T + tr("省 域 ID：0x%1(%2)<br>").arg(data.mid(0, 4))
                .arg(jtProtDlg->getProvinceId(province)));
            parsed.append(CTAB_T + tr("市县域ID：0x%1<br>").arg(data.mid(4, 4)));
            parsed.append(CTAB_T + tr("制造商ID：%1<br>").arg(manufacturerid));
            parsed.append(CTAB_T + tr("终端型号：%1<br>").arg(devtype));
            parsed.append(CTAB_T + tr("终 端 ID：%1<br>").arg(devid));
            
            parsed.append(CTAB_T + tr("车牌颜色：0x%1").arg(data.mid(72, 2)));
            uint8_t colour = data.mid(72, 2).toInt(nullptr, 16);
            if (0x00 == colour) parsed.append(tr("(无)"));
            else if (0x01 == colour) parsed.append(tr("(蓝色)"));
            else if (0x02 == colour) parsed.append(tr("(黄色)"));
            else if (0x03 == colour) parsed.append(tr("(黑色)"));
            else if (0x02 == colour) parsed.append(tr("(白色)"));
            else parsed.append(tr("(其他)"));
            parsed.append(tr("<br>"));
            
            parsed.append(CTAB_T + tr("车牌/VIN：%1<br>").arg(platevin));
            if (data.length() <= 108) {
                break;
            }
            
            parsed.append(CTAB_T + tr("企业编码：%1").arg(companyid));
            if ("JAC" == companyid) parsed.append(tr("(江淮)"));
            else if ("JMC" == companyid) parsed.append(tr("(江铃)"));
            else if ("FOTON" == companyid) parsed.append(tr("(福田)"));
            else parsed.append(tr("(未知)"));
            parsed.append(tr("<br>"));

            parsed.append(CTAB_T + tr("摘要算法：0x%1").arg(data.mid(124, 2)));
            uint8_t digest = data.mid(124, 2).toInt(nullptr, 16);
            if (0x01 == digest) parsed.append(tr("(SM3)"));
            else if (0x02 == digest) parsed.append(tr("(SHA256)"));
            else parsed.append(tr("(未知)"));
            parsed.append(tr("<br>"));

            parsed.append(CTAB_T + tr("加密算法：0x%1").arg(data.mid(126, 2)));
            uint8_t encryption = data.mid(126, 2).toInt(nullptr, 16);
            if (0x01 == encryption) parsed.append(tr("(SM2)"));
            else if (0x02 == encryption) parsed.append(tr("(RSA)"));
            else parsed.append(tr("(未知)"));
            parsed.append(tr("<br>"));
            
            parsed.append(CTAB_T + tr("密钥长度：0x%1(%2)<br>").arg(data.mid(128, 2)).arg(keylen));
            parsed.append(CTAB_T + tr("密钥内容：%1<br>").arg(data.mid(130, keylen * 2)));
            
            int chipidloc = 130 + keylen * 2;
            uint8_t chipidlen = data.mid(chipidloc, 2).toInt(nullptr, 16);
            QString chipid = hexStr2ByteArray(data.mid(chipidloc + 2, chipidlen * 2));
            parsed.append(CTAB_T + tr("芯片ID长度：0x%1(%2)<br>").arg(data.mid(chipidloc, 2)).arg(chipidlen));
            parsed.append(CTAB_T + tr("芯片ID内容：%1<br>").arg(chipid));

            int chipmakersloc = chipidloc + 2 + chipidlen * 2;
            uint8_t chipmakerslen = data.mid(chipmakersloc, 2).toInt(nullptr, 16);
            QString makers = hexStr2ByteArray(data.mid(chipmakersloc + 2, chipmakerslen * 2));
            parsed.append(CTAB_T + tr("芯片厂商长度：0x%1(%2)<br>").arg(data.mid(chipmakersloc, 2)).arg(chipmakerslen));
            parsed.append(CTAB_T + tr("芯片厂商内容：%1<br>").arg(makers));

            if (data.length() >= (chipmakersloc + 2 + chipmakerslen * 2 + 40)) {
                QString iccid = hexStr2ByteArray(data.right(40));
                parsed.append(CTAB_T + tr("手机卡ICCID：%1<br>").arg(iccid));
            }
            break;
        }
        case UP_AULOG: {
            QString authcode = hexStr2ByteArray(data);
            parsed.append(CTAB_T + tr("鉴权码：%1<br>").arg(authcode));
            break;
        }
        case UP_POSITION: {
            uint32_t alarmflag = data.mid(0, 8).toULong(nullptr, 16);
            uint32_t status = data.mid(8, 8).toULong(nullptr, 16);
            uint32_t latitude = data.mid(16, 8).toULong(nullptr, 16);
            uint32_t longitude = data.mid(24, 8).toULong(nullptr, 16);
            uint16_t height = data.mid(32, 4).toUInt(nullptr, 16);
            uint16_t speed = data.mid(36, 4).toUInt(nullptr, 16);
            uint16_t direction = data.mid(40, 4).toUInt(nullptr, 16);
            int year = data.mid(44, 2).toUShort(nullptr, 10);
            int month = data.mid(46, 2).toUShort(nullptr, 10);
            int day = data.mid(48, 2).toUShort(nullptr, 10);
            int hour = data.mid(50, 2).toUShort(nullptr, 10);
            int minute = data.mid(52, 2).toUShort(nullptr, 10);
            int second = data.mid(54, 2).toUShort(nullptr, 10);
            parsed.append(CTAB_T + tr("报警标志：0x%1(%2)<br>").arg(data.mid(0, 8), jtProtDlg->getAlarmFlag(alarmflag)));
            parsed.append(CTAB_T + tr("状态：0x%1(%2)<br>").arg(data.mid(8, 8), jtProtDlg->getStatus(status)));
           // Q_REQUIRED_RESULT QString arg(double a, int fieldWidth = 0, char fmt = 'g', int prec = -1,
           //     QChar fillChar = QLatin1Char(' ')) const;
            parsed.append(CTAB_T + tr("纬度：0x%1(%2 10的6次方)<br>").arg(data.mid(16, 8))
                .arg((double)(latitude/1000000.0), -1, 'g', 10));
            parsed.append(CTAB_T + tr("经度：0x%1(%2 10的6次方)<br>").arg(data.mid(24, 8))
                .arg((double)(longitude/1000000.0), -1, 'g', 10));
            parsed.append(CTAB_T + tr("高程：0x%1(%2 海拔高度,单位为米(m))<br>").arg(data.mid(32, 4)).arg(height));
            parsed.append(CTAB_T + tr("速度：0x%1(%2 1/10km/h)<br>").arg(data.mid(36, 4)).arg(speed));
            parsed.append(CTAB_T + tr("方向：0x%1(%2 0-359,正北为0,顺时针)<br>").arg(data.mid(40, 4)).arg(direction));
            parsed.append(CTAB_T + tr("时间：%1-%2-%3 %4:%5:%6<br>").arg(year).arg(month, 2, 10, QChar('0'))
                .arg(day, 2, 10, QChar('0')).arg(hour, 2, 10, QChar('0'))
                .arg(minute, 2, 10, QChar('0')).arg(second, 2, 10, QChar('0')));

            parsed.append(CTAB_T + tr("附加信息：{<br>"));
            QString addinfo = data.mid(56);
            for (int i = 0; i < addinfo.length(); ) {
                uint8_t addtype = addinfo.mid(i, 2).toUShort(nullptr, 16);
                uint8_t len = addinfo.mid(i + 2, 2).toUShort(nullptr, 16);
                switch (addtype) {
                    case 0x01: {
                        uint32_t value = addinfo.mid(i + 4, len*2).toULong(nullptr, 16);
                        parsed.append(CTAB_2T + tr("里程0x01：0x%1(%2 1/10km)<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x02: {
                        uint16_t value = addinfo.mid(i + 4, len*2).toUInt(nullptr, 16);
                        parsed.append(CTAB_2T + tr("油量0x02：0x%1(%2 1/10L)<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x03: {
                        uint16_t value = addinfo.mid(i + 4, len*2).toUInt(nullptr, 16);
                        parsed.append(CTAB_2T + tr("速度0x03：0x%1(%2 1/10km/h)<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x04: {
                        uint16_t value = addinfo.mid(i + 4, len*2).toUInt(nullptr, 16);
                        parsed.append(CTAB_2T + tr("人工报警事件0x04：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x11: {
                        uint64_t value = addinfo.mid(i + 4, len*2).toULongLong(nullptr, 16);
                        parsed.append(CTAB_2T + tr("超速报警0x11：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x12: {
                        uint64_t value = addinfo.mid(i + 4, len*2).toULongLong(nullptr, 16);
                        parsed.append(CTAB_2T + tr("进出区域/路线报警0x12：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x13: {
                        uint64_t value = addinfo.mid(i + 4, len*2).toULongLong(nullptr, 16);
                        parsed.append(CTAB_2T + tr("路段行驶时间不足/过长报警0x13：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x25: {
                        uint32_t value = addinfo.mid(i + 4, len*2).toULong(nullptr, 16);
                        parsed.append(CTAB_2T + tr("扩展车辆信号状态位0x25：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x2A: {
                        uint16_t value = addinfo.mid(i + 4, len*2).toUInt(nullptr, 16);
                        parsed.append(CTAB_2T + tr("IO状态位0x2A：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x2B: {
                        parsed.append(CTAB_2T + tr("模拟量0x2B：0x%1(bit0-15,AD0; bit16-31,AD1)<br>")
                            .arg(addinfo.mid(i + 4, len*2)));
                        break;
                    }
                    case 0x30: {
                        uint8_t value = addinfo.mid(i + 4, len*2).toUShort(nullptr, 16);
                        parsed.append(CTAB_2T + tr("无线信号强度0x30：0x%1(%2)<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    case 0x31: {
                        uint16_t value = addinfo.mid(i + 4, len*2).toUInt(nullptr, 16);
                        parsed.append(CTAB_2T + tr("GNSS定位卫星数0x31：0x%1(%2 )<br>")
                            .arg(addinfo.mid(i + 4, len*2)).arg(value));
                        break;
                    }
                    default: {
                        parsed.append(CTAB_2T + tr("ID:%1,LEN:%2,DATA：%3<br>").arg(addinfo.mid(i, 2))
                            .arg(len).arg(addinfo.mid(i + 4, len * 2)));
                        break;
                    }
                }
                i += 4 + len * 2;
            }
            parsed.append(CTAB_T + tr("}"));
            break;
        }
        case COMM_ACK: {
            parsed.append(CTAB_T + tr("应答流水号：0x%1<br>").arg(data.mid(0, 4)));
            parsed.append(CTAB_T + tr("应答ID：0x%1<br>").arg(data.mid(4, 4)));
            parsed.append(CTAB_T + tr("结果：0x%1").arg(data.mid(8, 2)));
            uint8_t result = data.mid(8, 2).toInt(nullptr, 16);
            if (0x00 == result) parsed.append(tr("(成功/确认)"));
            else if (0x01 == result) parsed.append(tr("(失败)"));
            else if (0x02 == result) parsed.append(tr("(消息有误)"));
            else if (0x03 == result) parsed.append(tr("(不支持)"));
            else parsed.append(tr("(未知)"));
            break;
        }
        default: {
            parsed.append(CTAB_T + tr("待解析数：<br>") + tr("{%1}<br>").arg(data));
            break;
        }
    }

    return parsed;
}

QString ParseCommandDialog::jiaoTong2_parsedServerData(uint16_t command, const QString &data)
{
    QString parsed;
    switch (command & 0x07FF) {
        case UP_REG: {
            parsed.append(CTAB_T + tr("应答流水号：0x%1<br>").arg(data.mid(0, 4)));
            parsed.append(CTAB_T + tr("结果：0x%1").arg(data.mid(4, 2)));
            uint8_t result = data.mid(4, 2).toInt(nullptr, 16);
            if (0x00 == result) parsed.append(tr("(成功)"));
            else if (0x01 == result) parsed.append(tr("(车辆已被注册)"));
            else if (0x02 == result) parsed.append(tr("(数据库中无该车辆)"));
            else if (0x03 == result) parsed.append(tr("(终端已被注册)"));
            else if (0x04 == result) parsed.append(tr("(数据库中无该终端)"));
            else parsed.append(tr("(未知)"));
            parsed.append(tr("<br>"));
            QString authcode = hexStr2ByteArray(data.mid(6, data.length()));
            parsed.append(CTAB_T + tr("鉴权码：%1").arg(authcode));
            break;
        }
        case UP_AULOG:
            break;
        case COMM_ACK: {
            parsed.append(CTAB_T + tr("应答流水号：0x%1<br>").arg(data.mid(0, 4)));
            parsed.append(CTAB_T + tr("应答ID：0x%1<br>").arg(data.mid(4, 4)));
            parsed.append(CTAB_T + tr("结果：0x%1").arg(data.mid(8, 2)));
            uint8_t result = data.mid(8, 2).toInt(nullptr, 16);
            if (0x00 == result) parsed.append(tr("(成功/确认)"));
            else if (0x01 == result) parsed.append(tr("(失败)"));
            else if (0x02 == result) parsed.append(tr("(消息有误)"));
            else if (0x03 == result) parsed.append(tr("(不支持)"));
            else if (0x04 == result) parsed.append(tr("(报警处理确认)"));
            else parsed.append(tr("(未知)"));
            parsed.append(tr("<br>"));
            break;
        }
        default: {
            parsed.append(CTAB_T + tr("待解析数：<br>") + tr("{%1}<br>").arg(data));
            break;
        }
    }

    return parsed;
}

QString ParseCommandDialog::jiaoTong2_parsedData(uint8_t dev,       uint16_t command, const QString &data)
{
    QString parsed;
    qDebug() << "dev" << dev << "command" << command << "data" << data;
    switch (dev) {
        case 'T':
            parsed = jiaoTong2_parsedClientData(command, data);
            break;
        case 'S':
            parsed = jiaoTong2_parsedServerData(command, data);
            break;
        default: break;
    }

    return parsed;
}

QString ParseCommandDialog::protocolParseJiaoTong2(QString &parse)
{
    QString tmp = parse.replace(QRegExp("[^(0-9a-fA-F)]"), "");
    QString deassembled = jtProtDlg->deassembleToHex(parse).toHex().toUpper();
    //qDebug() << "deassembled" << deassembled;

    uint16_t command = deassembled.mid(0, 4).toInt(nullptr, 16);
    uint16_t len = deassembled.mid(4, 4).toInt(nullptr, 16);
    uint16_t seq = deassembled.mid(20, 4).toInt(nullptr, 16);
    qDebug() << "command" << command << "len" << len << "seq" << seq;

    QString parsed;
    parsed.append(tr("消 息 ID：0x%1").arg(deassembled.mid(0, 4)) + CTAB_T);
    parsed.append(tr("消息体属性：0x%1(%2)<br>").arg(deassembled.mid(4, 4)).arg(len));
    parsed.append(tr("终端手机号：%1").arg(deassembled.mid(8, 12)) + CTAB_T);
    parsed.append(tr("消息流水号：0x%1(%2)<br>").arg(deassembled.mid(20, 4)).arg(seq));
    parsed.append(tr("消息报文内容：{<br>%1<br>}<br>").arg(jiaoTong2_parsedData(
        command & 0x8000 ? 'S' : 'T', command, deassembled.mid(24, len * 2))));
    parsed.append(tr("校验和：0x%1<br><br>").arg(deassembled.mid(24 + len * 2, 2)));

    if (command & 0x8000) {
        parsed.insert(0, "*************** 交通部（第二版）服务端报文 ***************<br><br>");
    } else {
        parsed.insert(0, "*************** 交通部（第二版）客服端报文 ***************<br><br>");
    }
    
    return parsed;
}

// --------- 北斗车载终端重汽定制协议 ----------//

QString ParseCommandDialog::zhongQi_parsedClientData(       uint16_t command, const QString &data)
{
    QString parsed;
    switch (command) {
        case UP_ZQ_LOG: {
            QString hexstr = hexStr2ByteArray(data);
            QStringList list = hexstr.split(',');
            if (list.size() < 5) {
                break;
            }
            parsed.append(CTAB_T + tr("P文件号：%1<br>").arg(list.at(0)));
            parsed.append(CTAB_T + tr("ICCID  ：%1<br>").arg(list.at(1)));
            parsed.append(CTAB_T + tr("版本号 ：%1<br>").arg(list.at(2)));
            parsed.append(CTAB_T + tr("车辆VIN：%1<br>").arg(list.at(3)));
            parsed.append(CTAB_T + tr("手机号 ：%1<br>").arg(list.at(4)));
            break;
        }
        case UP_ZQ_FA01: {
            parsed.append(CTAB_T + tr("单元体数量：0x%1<br>").arg(data.mid(0, 2)));
            parsed.append(CTAB_T + tr("单元体字节数：0x%1<br>").arg(data.mid(2, 2)));
            parsed.append(CTAB_T + tr("版本号：0x%1<br>").arg(data.mid(4, 2)));
            parsed.append(CTAB_T + tr("日期时间：20%1-%2-%3 %4:%5:%6<br>").arg(data.mid(6, 2),
                data.mid(8, 2), data.mid(10, 2), data.mid(12, 2),
                data.mid(14, 2), data.mid(16, 2)));
            parsed.append(CTAB_T + tr("定位有效性：%1<br>").arg("41" == data.mid(18, 2)? "有效" : "无效"));
            QString longitude = data.mid(26, 2) + data.mid(24, 2) + data.mid(22, 2) + data.mid(20, 2);
            parsed.append(CTAB_T + tr("经度：0x%1(s=%2 d=%3 公式d=s x 0.000001 + (-180))<br>").arg(longitude).arg(
                longitude.toULong(nullptr, 16)).arg(((double)longitude.toULong(nullptr, 16))/1000000-180));
            QString latitude = data.mid(34, 2) + data.mid(32, 2) + data.mid(30, 2) + data.mid(28, 2);
            parsed.append(CTAB_T + tr("纬度：0x%1(s=%2 d=%3 公式d=s x 0.000001 + (-90))<br>").arg(latitude).arg(
                latitude.toULong(nullptr, 16)).arg(((double)latitude.toULong(nullptr, 16))/1000000-90));
            uint8_t speed = data.mid(36, 2).toInt(nullptr, 16);
            parsed.append(CTAB_T + tr("速度：0x%1(%2 范围：0-200 km/h)<br>").arg(data.mid(36, 2)).arg(speed));
            QString direction = data.mid(40, 2) + data.mid(38, 2);
            parsed.append(CTAB_T + tr("方向：0x%1(%2 范围：0-359)<br>").arg(direction).arg(direction.toInt(nullptr, 16)));
            QString altitude = data.mid(44, 2) + data.mid(42, 2);
            parsed.append(CTAB_T + tr("海拔：0x%1(%2 范围：0-60000)<br>").arg(altitude).arg(altitude.toInt(nullptr, 16)));
            uint8_t gpsNum = data.mid(46, 2).toInt(nullptr, 16);
            parsed.append(CTAB_T + tr("卫星数：%1(%2 范围：0-50)<br>").arg(data.mid(46, 2)).arg(gpsNum));
            uint8_t rssi = data.mid(48, 2).toInt(nullptr, 16);
            parsed.append(CTAB_T + tr("GSM 通讯信号强度：0x%1(%2)<br>").arg(data.mid(48, 2)).arg(rssi));
            parsed.append(CTAB_T + tr("CAN 数据有效性标识符：%1<br>").arg("41" == data.mid(50, 2)? "有效" : "无效"));
            QString ccvsspeed = data.mid(54, 2) + data.mid(52, 2);
            parsed.append(CTAB_T + tr("CCVS 车速：0x%1(%2 范围：0-60000)<br>").arg(ccvsspeed).arg(ccvsspeed.toInt(nullptr, 16)));
            break;
        }
        default: {
            QString hexstr = hexStr2ByteArray(data.mid(0, 4));
            parsed.append(tr("%1:").arg(hexstr));
            parsed.append(tr("%1").arg(data.right(data.length() - 4)));
            break;
        }
    }

    return parsed;
}

QString ParseCommandDialog::zhongQi_parsedServerData(uint16_t command, const QString &data)
{
    QString parsed;
    switch (command) {
        case UP_ZQ_LOG: { // 4F4B20210902093940
            QString hexstr = hexStr2ByteArray(data.mid(0, 4));
            parsed.append(CTAB_T + tr("%1<br>").arg(hexstr));
            parsed.append(CTAB_T + tr("时间：%1年%2月%3日 %4时%5分%6秒<br>").arg(data.mid(4, 4),
                data.mid(8, 2), data.mid(10, 2), data.mid(12, 2), data.mid(14, 2),
                data.mid(16, 2)));
            break;
        }
        case UP_ZQ_FA01:
            break;
        default: {
            parsed.append(CTAB_T + tr("%1<br>").arg(data));
            break;
        }
    }

    return parsed;
}

QString ParseCommandDialog::zhongQi_parsedData(uint8_t dev,       uint16_t command, const QString &data)
{
    QString parsed;
    qDebug() << "dev" << dev << "command" << command << "data" << data;
    switch (dev) {
        case 'T':
            parsed = zhongQi_parsedClientData(command, data);
            break;
        case 'S':
            parsed = zhongQi_parsedServerData(command, data);
            break;
        default: break;
    }

    return parsed;
}

QString ParseCommandDialog::protocolParseZhongQi(QString &parse)
{
    QString tmp = parse.replace(QRegExp("[^(0-9a-fA-F)]"), "");
    QString deassembled = zqProtDlg->deassembleToHex(parse).toHex().toUpper();
    //qDebug() << "deassembled" << deassembled;

    QString compress = deassembled.mid(18, 2);
    uint8_t dev = deassembled.mid(8, 2).toInt(nullptr, 16);
    uint16_t len = deassembled.mid(26, 4).toInt(nullptr, 16);
    uint16_t command = deassembled.mid(22, 4).toInt(nullptr, 16);

    QString parsed;
    parsed.append(tr("SOH ：0x%1").arg(deassembled.mid(0, 2)) + CTAB_T);
    parsed.append(tr("DLE ：0x%1").arg(deassembled.mid(2, 2)) + CTAB_T);
    parsed.append(tr("包序号：0x%1<br><br>").arg(deassembled.mid(4, 4)));
    parsed.append(tr("设备：0x%1(%2)").arg(deassembled.mid(8, 2)).arg(
        0x54 == dev ? "T" : (0x53 == dev ? "S" : "Unknown")) + CTAB_T);
    parsed.append(tr("设备ID：0x%1<br>").arg(deassembled.mid(10, 8)));
    parsed.append(tr("压缩标识：0x%1(%2)").arg(compress).arg("00" == compress ? "不压缩" : "压缩") + CTAB_T);
    parsed.append(tr("车型：0x%1<br>").arg(deassembled.mid(20, 2)));
    parsed.append(tr("命 令 字：0x%1<br>").arg(deassembled.mid(22, 4)));
    parsed.append(tr("数据长度：0x%1(%2)<br>").arg(deassembled.mid(26, 4)).arg(len));
    parsed.append(tr("数据报文内容：{<br>%1<br>}<br>").arg(
        zhongQi_parsedData(dev, command, deassembled.mid(30, len * 2))));
    parsed.append(tr("校验和：0x%1<br><br>").arg(deassembled.mid(30 + len * 2, 2)));
    parsed.append(tr("EOT ：0x%1<br>").arg(deassembled.mid(30 + len * 2 + 2, 2)));

    if (0x54 == dev) {
        parsed.insert(0, "************** 重汽定制客服端报文 **************<br><br>");
    } else if (0x53 == dev) {
        parsed.insert(0, "************** 重汽定制服务端报文 **************<br><br>");
    } else {
        parsed.insert(0, "************** 重汽定制未知端报文 **************<br><br>");
    }

    return parsed;
}


// --------- 解析协议命令入口 ----------//

void ParseCommandDialog::doTextChanged()
{
    QString parsed;
    QString parse = ui->parseTextEdit->toPlainText();
    if (m_protocol.contains("交通部标准协议(第二版)")) {
        parsed = protocolParseJiaoTong2(parse);
        jtProtDlg = static_cast<JtProtocolDialog *>(m_obj);
    } else if (m_protocol.contains("北斗车载终端重汽定制协议")) {
        parsed = protocolParseZhongQi(parse);
        zqProtDlg = static_cast<ZqProtocolDialog *>(m_obj);
    } else if (m_protocol.contains("国六(HJ1239)终端排放协议")) {
        parsed = protocolParseG6HJ1239(parse);
    } else if (m_protocol.contains("宁德时代(RDB)私有协议")) {
        parsed = protocolParseNingDeShiDaiRDB(parse);
    }

    ui->parsedTextBrowser->clear();
    ui->parsedTextBrowser->insertHtml(parsed);
}

