#include "websoaptest.h"
#include "ui_websoaptest.h"
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>

WebSoapTest::WebSoapTest(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WebSoapTest)
    , m_netManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &WebSoapTest::onReplyFinished);
}

WebSoapTest::~WebSoapTest()
{
    delete ui;
}

void WebSoapTest::on_pushButton_clicked()
{
    QString urlStr = ui->lineEditUrl->text().trimmed();
    if (urlStr.isEmpty()) {
        QMessageBox::warning(this, "Error", "URL cannot be empty.");
        return;
    }

    QString soapXml = ui->textEditRequest->toPlainText().trimmed();
    if (soapXml.isEmpty()) {
        QMessageBox::warning(this, "Error", "SOAP request body cannot be empty.");
        return;
    }

    // 构建请求
    QNetworkRequest request((QUrl(urlStr)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=utf-8");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/soap+xml; charset=utf-8");
    // 某些 WebService 还需要 SOAPAction 头，如有需要可取消注释并设置
    // request.setRawHeader("SOAPAction", "http://tempuri.org/YourMethod");

    ui->labelstatus->setText("Sending...");
    ui->pushButton->setEnabled(false);
    ui->textEditRequest->clear();

    // 发送 POST
    m_netManager->post(request, soapXml.toUtf8());
}
void WebSoapTest::onReplyFinished(QNetworkReply *reply)
{
    ui->pushButton->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        ui->labelstatus->setText("Error");
        ui->textEditGet->setText(QString("Network Error: %1\n%2")
                                    .arg(reply->errorString())
                                    .arg(reply->readAll()));
        reply->deleteLater();
        return;
    }

    // 成功
    ui->labelstatus->setText("Success");
    // 获取响应数据
    QByteArray responseData = reply->readAll();
    // 尝试以 UTF-8 显示
    QString responseText = QString::fromUtf8(responseData);
    if (responseText.isEmpty()) {
        responseText = QString::fromLatin1(responseData); // fallback
    }
    ui->textEditGet->setText(responseText);

    reply->deleteLater();
}

