#ifndef ZQDATABASEDIALOG_H
#define ZQDATABASEDIALOG_H

#include <QDialog>

namespace Ui {
class ZqDatabaseDialog;
}

class QSqlQueryModel;

class ZqDatabaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZqDatabaseDialog(QWidget *parent = 0);
    ~ZqDatabaseDialog();

public slots:
    void doDatabaseChanged();

private:
    Ui::ZqDatabaseDialog *ui;
    QSqlQueryModel *model;
};

#endif // ZQDATABASEDIALOG_H
