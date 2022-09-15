#ifndef JTDATABASEDIALOG_H
#define JTDATABASEDIALOG_H

#include <QDialog>

namespace Ui {
class JtDatabaseDialog;
}

class QSqlQueryModel;

class JtDatabaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JtDatabaseDialog(QWidget *parent = 0);
    ~JtDatabaseDialog();

public slots:
    void doDatabaseChanged();

private:
    Ui::JtDatabaseDialog *ui;
    QSqlQueryModel *model;
};

#endif // JTDATABASEDIALOG_H
