#include "zqdatabasedialog.h"
#include "ui_zqdatabasedialog.h"

#include <QSqlQueryModel>
#include <QSortFilterProxyModel>

ZqDatabaseDialog::ZqDatabaseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZqDatabaseDialog)
{
    ui->setupUi(this);

    model = new QSqlQueryModel(this);
    QSortFilterProxyModel *sqlproxy = new QSortFilterProxyModel(this);
    sqlproxy->setSourceModel(model);

    ui->tableView->setModel(sqlproxy);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->verticalHeader()->hide();
    model->setQuery("select * from zqdevinfo order by id asc");

    int column = 0;
    model->setHeaderData(column++, Qt::Horizontal, tr("序号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("设备ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("ICCID号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("P文件号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("车辆VIN号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SIM卡号"));
    model->setHeaderData(column++, Qt::Horizontal, tr("版本号"));

    column = 0;
    ui->tableView->setColumnWidth(column++, 60);
    ui->tableView->setColumnWidth(column++, 80);
    ui->tableView->setColumnWidth(column++, 160);
    ui->tableView->setColumnWidth(column++, 200);
    ui->tableView->setColumnWidth(column++, 160);
    ui->tableView->setColumnWidth(column++, 110);
    ui->tableView->setColumnWidth(column++, 240);
}

ZqDatabaseDialog::~ZqDatabaseDialog()
{
    delete ui;
}

void ZqDatabaseDialog::doDatabaseChanged()
{
    model->setQuery("select * from zqdevinfo order by id asc");
}

