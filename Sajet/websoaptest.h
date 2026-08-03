#ifndef WEBSOAPTEST_H
#define WEBSOAPTEST_H

#include <QWidget>
#include <QNetworkAccessManager>

namespace Ui {
class WebSoapTest;
}

class WebSoapTest : public QWidget
{
    Q_OBJECT

public:
    explicit WebSoapTest(QWidget *parent = nullptr);
    ~WebSoapTest();

private slots:
    void on_pushButton_clicked();
    void onReplyFinished(QNetworkReply *reply);

private:
    Ui::WebSoapTest *ui;
    QNetworkAccessManager *m_netManager;
};

#endif // WEBSOAPTEST_H
