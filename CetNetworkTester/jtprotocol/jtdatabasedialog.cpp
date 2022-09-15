#include "jtdatabasedialog.h"
#include "ui_jtdatabasedialog.h"

#include <QSqlQueryModel>
#include <QSortFilterProxyModel>

#define WIDTH_ID                60
#define WIDTH_SIMID             120
#define WIDTH_PROVINCEID        80
#define WIDTH_CITYID            100
#define WIDTH_MANFACTURERID     100
#define WIDTH_DEVTYPEID         120
#define WIDTH_DEVID             100
#define WIDTH_COLOUR            80
#define WIDTH_PLATE_VIN         180

JtDatabaseDialog::JtDatabaseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::JtDatabaseDialog)
{
    ui->setupUi(this);

    model = new QSqlQueryModel(this);
    QSortFilterProxyModel *sqlproxy = new QSortFilterProxyModel(this);
    sqlproxy->setSourceModel(model);

    ui->tableView->setModel(sqlproxy);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->verticalHeader()->hide();
    model->setQuery("select * from jt2devinfo order by id asc");

    int column = 0;
    model->setHeaderData(column++, Qt::Horizontal, tr("序号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SIM卡号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("省域ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("市县域ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("制造商ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("设备类型"));
    model->setHeaderData(column++, Qt::Horizontal, tr("设备ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("车牌颜色"));
    model->setHeaderData(column++, Qt::Horizontal, tr("车牌/VIN"));
    model->setHeaderData(column++, Qt::Horizontal, tr("企业编码"));
    model->setHeaderData(column++, Qt::Horizontal, tr("摘要算法"));
    model->setHeaderData(column++, Qt::Horizontal, tr("加密算法"));
    model->setHeaderData(column++, Qt::Horizontal, tr("密钥内容"));
    model->setHeaderData(column++, Qt::Horizontal, tr("芯片ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("芯片厂商"));
    model->setHeaderData(column++, Qt::Horizontal, tr("ICCID号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("鉴权码"));

    column = 0;
    ui->tableView->setColumnWidth(column++, WIDTH_ID);
    ui->tableView->setColumnWidth(column++, WIDTH_SIMID);
    ui->tableView->setColumnWidth(column++, WIDTH_PROVINCEID);
    ui->tableView->setColumnWidth(column++, WIDTH_CITYID);
    ui->tableView->setColumnWidth(column++, WIDTH_MANFACTURERID);
    ui->tableView->setColumnWidth(column++, WIDTH_DEVTYPEID);
    ui->tableView->setColumnWidth(column++, WIDTH_DEVID); // 制造商ID
    ui->tableView->setColumnWidth(column++, WIDTH_COLOUR);
    ui->tableView->setColumnWidth(column++, WIDTH_PLATE_VIN);
}

JtDatabaseDialog::~JtDatabaseDialog()
{
    delete ui;
}

void JtDatabaseDialog::doDatabaseChanged()
{
    model->setQuery("select * from jt2devinfo order by id asc");
}

