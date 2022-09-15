#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtNetwork>
#include <QMessageBox>
#include <QPainter>

#include "version.h"

#define SIGN_TX "TX→◇ "
#define SIGN_RX "RX←◆ "

#if (IS_RELEASE_VERSION > 0)
#define VERSION_TIP "正式版"
#else
#define VERSION_TIP "测试版"
#endif

#define CET_DEV_NAME "Cet网络测试器"
#define CETNETWORKTESTER_VERSION CET_DEV_NAME " V" PRODUCT_VERSION_STR " " VERSION_TIP
#define CETNETWORKTESTER_SETUP 	"CetNetworkTester-Setup"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    protType(PROTOCOL_UNKNOWN),
    m_cetLicIface(nullptr),
    m_cetUpIface(nullptr),
    recvcount(0),
    sendcount(0)
{
    ui->setupUi(this);
    setWindowTitle(tr(CETNETWORKTESTER_VERSION));

    ui->seqAddButton->setEnabled(false);
    ui->sendButton->setEnabled(false);
    ui->timingSendButton->setEnabled(false);
    connect(ui->protocolTypeSelectComboBox, static_cast<void (QComboBox:: *)(int)>
        (QComboBox::currentIndexChanged), this, MainWindow::doProtocolSelectChanged);
    
    QPalette palette = ui->socketStatusLabel->palette();
    palette.setColor(QPalette::WindowText, QColor(0, 0, 255));
    ui->socketStatusLabel->setPalette(palette);

    socket = new QTcpSocket(this);
    //socket->setPrivateKey(QSslKey(hexStr2ByteArray("B1215BEFC4FF259CC26782A3C1900E0BAB0529C929DCD4BA4ED327B14C805388A8D674C147F048CB69A903A270B5E49E88AEFF3279EB53B077CF8506326962EC")));
    connect(socket, QAbstractSocket::connected, this, &MainWindow::doConnected);
    connect(socket, &QIODevice::readyRead, this, &MainWindow::doReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(QAbstractSocket::error),
        this, &MainWindow::displayError);

    conntimer = new QTimer(this);
    connect(conntimer, QTimer::timeout, this, MainWindow::doTimeoutConnect);
    sendtimer = new QTimer(this);
    connect(sendtimer, QTimer::timeout, this, MainWindow::doTimeoutSend);

    connecttip = new QMessageBox(this);
    connecttip->setWindowTitle(tr("连接服务器"));
    connecttip->setText(tr("\t正在连接服务器中\t\t"));
    connecttip->setStandardButtons(QMessageBox::Cancel);
    connecttip->setButtonText(QMessageBox::Cancel, tr("取消"));

    zqProtDlg = new ZqProtocolDialog(this);
    connect(ui->zqConfigButton, QPushButton::clicked, this, MainWindow::doZqProtocolDialog);
    connect(ui->zqGenerateButton, QPushButton::clicked, this, MainWindow::doZqGenerateButton);
    connect(zqProtDlg, ZqProtocolDialog::baseInfoChanged, ui->zqBaseInfoLabel, QLabel::setText);

    jtProtDlg = new JtProtocolDialog(this);
    connect(ui->jtConfigButton, QPushButton::clicked, this, MainWindow::doJtProtocolDialog);
    connect(ui->jtGenerateButton, QPushButton::clicked, this, MainWindow::doJtGenerateButton);
    connect(jtProtDlg, JtProtocolDialog::baseInfoChanged, ui->jtBaseInfoLabel, QLabel::setText);

    parseCmdDlg = new ParseCommandDialog(this);

    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.beginGroup(QLatin1String("ServerInfo"));
    for (int i = 0; ; ++i) {
        QString server = settings.value(QString("Server%1").arg(i)).toString();
        if (server.isEmpty()) break;
        ui->serverComboBox->addItem(server);
    }
    for (int i = 0; ; ++i) {
        QString port = settings.value(QString("Port%1").arg(i)).toString();
        if (port.isEmpty()) break;
        ui->portComboBox->addItem(port);
    }
    settings.endGroup();

    on_clearButton_clicked();
    doProtocolSelectChanged(0);

    ui->zqBaseInfoLabel->setText(zqProtDlg->baseInfo());
    ui->jtBaseInfoLabel->setText(jtProtDlg->baseInfo());

    initDllPlugin();
}

MainWindow::~MainWindow()
{
    delete socket;
    delete conntimer;
    delete sendtimer;
    delete connecttip;
    delete m_cetLicIface;
    delete m_cetUpIface;
    delete ui;
}

QObject *MainWindow::loadDllPlugin(const QString &dllname)
{
	QObject *plugin = nullptr;
    QDir pluginsDir("./plugins");

    if (pluginsDir.isEmpty()) {
        pluginsDir.setPath("D:/CetToolLibs/plugins");
    }

    foreach (const QString &fileName, pluginsDir.entryList(QDir::Files)) {
        //qDebug() << "fileName" << fileName << "absoluteFilePath(fileName)" << pluginsDir.absoluteFilePath(fileName);
        QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
        plugin = pluginLoader.instance();
        if (plugin) {
            plugin->setParent(this);
            if (0 == dllname.compare(fileName, Qt::CaseInsensitive)) {
                break;
            }
        }
    }

    return plugin;
}

void MainWindow::initDllPlugin()
{
    QFileInfo fileInfo("./change.log");
    if (fileInfo.size() < 64) {
        QFile file("./change.log");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QString details;
        details.append("/***********************************************************************************************\n");
        details.append(tr("* 项目名称：%1\n").arg(QString(CETNETWORKTESTER_SETUP).split('-').first()));
        details.append("* 项目简介：网络协议诊断应用程序\n");
        details.append("* 创建日期：2022.05.11\n");
        details.append("* 创建人员：郑建宇(CetXiyuan)\n");
        details.append("* 版权说明：本程序仅为个人兴趣开发，有需要者可以免费使用，但是请不要在网络上进行恶意传播\n");
        details.append("***********************************************************************************************/\n\n");

        file.seek(0);
        file.write(details.toUtf8());
        file.close();
    }

    m_cetLicIface = qobject_cast<CetLicenseInterface *>(loadDllPlugin("CetLicensePlugin.dll"));
    if (!m_cetLicIface) {
        QMessageBox::warning(this, tr("警告"), tr("缺少 CetLicensePlugin.dll 库\t"));
    }

    m_cetUpIface = qobject_cast<CetUpdateInterface *>(loadDllPlugin("CetUpdatePlugin.dll"));
    if (!m_cetUpIface) {
        QMessageBox::warning(this, tr("警告"), tr("缺少 CetUpdatePlugin.dll 库\t"));
    }

    if (!m_cetLicIface || CetLicenseInterface::ACTIVATE_OK != m_cetLicIface->activate(PRODUCT_NAME)) {
        this->centralWidget()->setEnabled(false);
        this->setWindowTitle(windowTitle().append(tr(" [已到期]")));
    }

    if (m_cetUpIface) {
        m_cetUpIface->checkUpdate(CETNETWORKTESTER_SETUP, PRODUCT_VERSION_STR, QCoreApplication::quit);
    }
}

void MainWindow::doProtocolSelectChanged(int index)
{
    switch (index) {
        case 0: // 交通部标准协议(第二版)
            protType = PROTOCOL_JT2_STANDARD;
            ui->jt2GroupBox->show();
            ui->zqGroupBox->hide();
            break;
        case 1: // 北斗车载终端重汽定制协议
            protType = PROTOCOL_ZQ_CUSTOM;
            ui->zqGroupBox->show();
            ui->jt2GroupBox->hide();
            ui->seqAddButton->setEnabled(true);
            break;
        case 2: // 国六(HJ1239)终端排放协议
            protType = PROTOCOL_G6_HJ1239;
            ui->jt2GroupBox->hide();
            ui->zqGroupBox->hide();
            break;
        case 3: // 宁德时代(RDB)私有协议
            protType = PROTOCOL_NDSD_RDB;
            ui->jt2GroupBox->hide();
            ui->zqGroupBox->hide();
            break;
        default: break;
    }
}

void MainWindow::doZqProtocolDialog()
{
    ui->zqBaseInfoLabel->setText(zqProtDlg->baseInfo());
    zqProtDlg->showFrame(ui->zqLoginFrameRadioButton->isChecked(),
            ui->zqDataFrameRadioButton->isChecked());
}

void MainWindow::doJtProtocolDialog()
{
    ui->jtBaseInfoLabel->setText(jtProtDlg->baseInfo());
    jtProtDlg->showFrame(ui->jtRegiterFrameRadioButton->isChecked(),
            ui->jtLoginFrameRadioButton->isChecked(),
            ui->jtLogoutFrameRadioButton->isChecked());
}

void MainWindow::doConnected()
{
    //QApplication::restoreOverrideCursor();

    ui->serverComboBox->setEnabled(false);
    ui->portComboBox->setEnabled(false);
    ui->sendButton->setEnabled(true);
    ui->timingSendButton->setEnabled(true);

    QString server = ui->serverComboBox->currentText();
    if (-1 == ui->serverComboBox->findText(server)) {
        ui->serverComboBox->addItem(server);
    }

    QString port = ui->portComboBox->currentText();
    if (-1 == ui->portComboBox->findText(port)) {
        ui->portComboBox->addItem(port);
    }
    
    ui->connectButton->setText(tr("断开"));
    ui->socketStatusLabel->setText(tr("状态：已连接 - 本机IP：%1 端口号：%2")
        .arg(socket->localAddress().toString()).arg(socket->localPort()));

    connecttime = 0;
    connecttip->hide();
    ui->connectTimeLabel->setText(tr("连接计时：00:00:00"));
}

void MainWindow::doReadyRead()
{
    if (!socket)
        return;

    QByteArray readdata = socket->readAll();
    qDebug() << "readdata" << readdata;
    QByteArray data = readdata.toHex().toUpper();
    recvcount += data.size() / 2;
    ui->recvLabel->setText(tr("接收：%1 字节").arg(recvcount));

    QString display;
    display.append(QString("[%1]%2").arg(QTime::currentTime().toString("hh:mm:ss.zzz"), SIGN_RX));
    display.append("16进制{");
    
    switch (protType) {
        case PROTOCOL_JT2_STANDARD:
            break;
        case PROTOCOL_ZQ_CUSTOM:
            if (data.mid(32, 4).contains("4F4B")) {
                display.append("OK:");
            } else {
                display.append("ER:");
            }
            break;
        default: break;
    }
    
    display.append(data);
    display.append("}");

    ui->recvTextBrowser->append(display);
}

void MainWindow::doTimeoutConnect()
{
    if (QAbstractSocket::ConnectedState == socket->state()) {
        ++connecttime;
        int hour = connecttime / 3600;
        int minute = (connecttime / 60) % 60;
        int second = connecttime % 60;
        
        ui->connectTimeLabel->setText(tr("连接计时：%1:%2:%3").arg(hour, 2, 10, QChar('0'))
            .arg(minute, 2, 10, QChar('0')).arg(second, 2, 10, QChar('0')));
    } else if (connecttip->isVisible()) {
        if (++connecttiptime == 1)
            connecttip->setText(tr("\t正在连接服务器中 .\t\t"));
        else if (connecttiptime == 2)
            connecttip->setText(tr("\t正在连接服务器中 . .\t\t"));
        else if (connecttiptime == 3)
            connecttip->setText(tr("\t正在连接服务器中 . . .\t\t"));
        else if (connecttiptime == 4)
            connecttip->setText(tr("\t正在连接服务器中 . . . .\t\t"));
        else if (connecttiptime == 5)
            connecttip->setText(tr("\t正在连接服务器中 . . . . .\t\t"));
        else if (connecttiptime == 6) {
            connecttiptime = 0;
            connecttip->setText(tr("\t正在连接服务器中 . . . . . .\t\t"));
        }
    } else {
        conntimer->stop();
    }
}

void MainWindow::doTimeoutSend()
{
    on_sendButton_clicked();
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    //QApplication::restoreOverrideCursor();

    conntimer->stop();
    connecttip->hide();
    ui->serverComboBox->setEnabled(true);
    ui->portComboBox->setEnabled(true);
    ui->connectButton->setText(tr("连接"));
    ui->socketStatusLabel->setText(tr("状态：连接异常 - %1").arg(socket->errorString()));
    QMessageBox::critical(this, tr(CET_DEV_NAME), 
        tr("出错: %1 - %2").arg(socketError).arg(socket->errorString()));
}

void MainWindow::on_connectButton_clicked()
{
    ui->socketStatusLabel->setText(tr("状态：未连接"));

    QString server = ui->serverComboBox->currentText();
    QString port = ui->portComboBox->currentText();
    if (server.isEmpty() || port.isEmpty()) {
        QMessageBox::information(this, tr(CET_DEV_NAME), tr("请输入服务器IP和端口号！"));
        return;
    }

    if ("连接" == ui->connectButton->text()) {
        //QApplication::setOverrideCursor(Qt::WaitCursor);
        socket->connectToHost(server, port.toInt());
        connecttiptime = 0;
        conntimer->start(1000);
        if (QMessageBox::Cancel == connecttip->exec()) {
            socket->abort();
            conntimer->stop();
            //QApplication::restoreOverrideCursor();
        }
    } else {
        ui->sendButton->setEnabled(false);
        ui->timingSendButton->setEnabled(false);
        
        ui->serverComboBox->setEnabled(true);
        ui->portComboBox->setEnabled(true);
        ui->connectButton->setText(tr("连接"));
        socket->disconnectFromHost();
    }
}

void MainWindow::on_sendButton_clicked()
{
    QString buffer = ui->sendTextEdit->toPlainText();
    if (buffer.isEmpty()) {
        QMessageBox::information(this, tr(CET_DEV_NAME), tr("无可发送的数据！"));
        return;
    }

    QString display;
    QByteArray send;
    if (ui->hexFormatCheckBox->isChecked()) {
        display.append("16进制{");
        buffer = buffer.simplified().replace(" ", "");
        for (int i = 0; i < buffer.length(); i += 2) {
            send.append(buffer.mid(i, 2).toUShort(nullptr, 16));
            display.append(buffer.mid(i, 2));
        }
    } else {
        display.append("字符{");
        send = buffer.toLocal8Bit();
        display.append(buffer);
    }
    display.append("}");

    display.insert(0, QString("[%1]%2").arg(QTime::currentTime().toString("hh:mm:ss.zzz"), SIGN_TX));

    qDebug() << "size:" << send.size() << "send:" << send;
    int count = ui->countSpinBox->value() + 1;
    for (int i = 0; i < count; ++i) {
        QString buffer = display;
        if (-1 == socket->write(send)) {
            QMessageBox::warning(this, tr("warning"), tr("发送失败：%1\t").arg(display));
            if ("停止定时发送" == ui->timingSendButton->text()) {
                emit ui->timingSendButton->clicked(true);
            }
            return;
        }
        sendcount += send.size();
        ui->sendLabel->setText(tr("发送：%1 字节").arg(sendcount));
        if (i) buffer.append(tr(" Repate:%1").arg(i));
        ui->recvTextBrowser->append(buffer);
    }
}

void MainWindow::on_timingSendButton_clicked()
{
    QString buffer = ui->sendTextEdit->toPlainText();
    if (buffer.isEmpty()) {
        QMessageBox::information(this, tr(CET_DEV_NAME), tr("无可发送的数据！"));
        return;
    }

    if ("启动定时发送" == ui->timingSendButton->text()) {
        int interval = ui->intervalMsLineEdit->text().toInt();
        if (interval) {
            ui->timingSendButton->setText(tr("停止定时发送"));
            sendtimer->start(interval);
        }
    } else {
        ui->timingSendButton->setText(tr("启动定时发送"));
        sendtimer->stop();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.clear();
    settings.beginGroup(QLatin1String("ServerInfo"));
    for (int i = 0; i < ui->serverComboBox->count(); ++i) {
        settings.setValue(QString("Server%1").arg(i), ui->serverComboBox->itemText(i));
    }

    for (int i = 0; i < ui->portComboBox->count(); ++i) {
        settings.setValue(QString("Port%1").arg(i), ui->portComboBox->itemText(i));
    }
    settings.endGroup();

    QMainWindow::closeEvent(event);
}


void MainWindow::on_clearButton_clicked()
{
    recvcount = sendcount = 0;
    ui->recvLabel->setText(tr("接收：0 字节"));
    ui->sendLabel->setText(tr("接收：0 字节"));
    ui->recvTextBrowser->clear();
}

void MainWindow::doZqGenerateButton()
{
    if (ui->zqLoginFrameRadioButton->isChecked()) {
        ui->sendTextEdit->setPlainText(zqProtDlg->loginFrame());
    } else if (ui->zqDataFrameRadioButton->isChecked()) {
        ui->sendTextEdit->setPlainText(zqProtDlg->dataFrame());
    }
}

void MainWindow::doJtGenerateButton()
{
    if (ui->jtRegiterFrameRadioButton->isChecked()) {
        ui->sendTextEdit->setPlainText(jtProtDlg->regiterFrame());
    } else if (ui->jtLoginFrameRadioButton->isChecked()) {
        ui->sendTextEdit->setPlainText(jtProtDlg->loginFrame());
    } else if (ui->jtLogoutFrameRadioButton->isChecked()) {
        ui->sendTextEdit->setPlainText(jtProtDlg->logoutFrame());
    }
}

void MainWindow::on_seqAddButton_clicked()
{
    QString buffer = ui->sendTextEdit->toPlainText();
    if (ui->hexFormatCheckBox->isChecked()) {
        buffer = buffer.simplified().replace(" ", "");
    }
    
    switch (protType) {
        case PROTOCOL_JT2_STANDARD:
            break;
        case PROTOCOL_ZQ_CUSTOM:
            ui->sendTextEdit->setText(zqProtDlg->seqAddFrame(buffer));
            break;
        default: break;
    }
}

void MainWindow::on_parseCommandButton_clicked()
{
    QString text = ui->protocolTypeSelectComboBox->currentText();
    switch (protType) {
        case PROTOCOL_JT2_STANDARD:
            parseCmdDlg->setProtocol(text, (QObject *)jtProtDlg);
            break;
        case PROTOCOL_ZQ_CUSTOM:
            parseCmdDlg->setProtocol(text, (QObject *)zqProtDlg);
            break;
        case PROTOCOL_G6_HJ1239:
        case PROTOCOL_NDSD_RDB:
            parseCmdDlg->setProtocol(text);
            break;
        default: break;
    }
    parseCmdDlg->show();
}
