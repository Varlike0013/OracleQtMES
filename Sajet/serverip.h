#ifndef SERVERIP_H
#define SERVERIP_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QStandardItemModel>

namespace Ui {
class ServerIp;
}

class ServerIp : public QWidget
{
    Q_OBJECT

public:
    explicit ServerIp(QWidget *parent = nullptr);
    ~ServerIp();

private slots:
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void load_serveriptree();
    void on_pushButton_clicked();

private:
    Ui::ServerIp *ui;
    QStandardItemModel *m_tableModel = nullptr;
    void init_table_view();
    void append_table_row(QString serverId,QString gatewayId,int index);
    QString get_ip_status(QString serverId, QString gatewayId, int index);
};

#endif // SERVERIP_H
