#include "changeinput.h"
#include "ui_changeinput.h"

ChangeInput::ChangeInput(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChangeInput)
{
    ui->setupUi(this);
}

ChangeInput::~ChangeInput()
{
    delete ui;
}
QStringList ChangeInput::getListString(int typeIn, const QString &input)
{
    QStringList items;
    switch (typeIn) {
    case 0: // 按空格分割（多个空格视为一个分隔符）
        items = input.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        break;
    case 2: // 调用解析函数
        items = parseRangeString(input);
        break;
    default:// 对于无法解析为列表的类型，返回空列表
        break;
    }
    return items;
}

QString ChangeInput::changeText(int typeIn, int typeOut, const QString &input)
{
    // 处理特殊输入类型
    if (typeIn == 1) {//从 C++ SQL 拼接提取纯 SQL
        QString cleaned = input;
        cleaned.remove(QRegularExpression("^\\s*QString\\s+sql\\s*=\\s*"));
        cleaned.remove(QRegularExpression("^\"|\"$"));
        cleaned.replace(QRegularExpression("\"\\s*\\+\\s*\""), "");
        cleaned.replace(QRegularExpression("\\+\\s*\""), "");
        cleaned.replace(QRegularExpression("\"\\s*\\+"), "");
        cleaned = cleaned.simplified();
        cleaned.remove('"');
        return cleaned;
    }else if (typeIn == 2 && typeOut == 1){// 处理范围输入并输出 BETWEEN 格式
        QString trimmed = input.trimmed();
        if (trimmed.contains('-')) {
            QStringList parts = trimmed.split('-', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                QString start = parts[0].trimmed();
                QString end = parts[1].trimmed();
                // 自动添加单引号
                if (!start.startsWith('\'')) start = "'" + start + "'";
                if (!end.startsWith('\'')) end = "'" + end + "'";
                return QString("BETWEEN %1 AND %2").arg(start).arg(end);
            }
        }
        return "INVALID RANGE";
    }

    // 对于其他输入类型，调用 getListString 解析为列表
    QStringList items = getListString(typeIn, input);
    if (items.isEmpty()) {
        return "()";   // 空列表输出空元组
    }

    // 根据输出类型格式化
    QString output;
    switch (typeOut) {
    case 0: { // 默认输出：('item1', 'item2', ...)
        QStringList quoted;
        for (const QString &s : items) {
            quoted << "'" + s + "'";
        }
        output = "(" + quoted.join(", ") + ")";
        break;
    }
    default:
        output = "Unknown output type";
        break;
    }
    return output;
}
void ChangeInput::on_pushButtonChange_clicked()
{
    QString input = ui->plainTextEditInput->toPlainText();
    if (input.isEmpty()) {
        ui->plainTextEditOutput->setPlainText("Input empty");
        return;
    }
    int typeIn = ui->comboBoxType->currentIndex();
    int typeOut = ui->comboBoxOut->currentIndex();

    QString output = changeText(typeIn,typeOut,input);
    // 5. 输出到结果文本框
    ui->plainTextEditOutput->setPlainText(output);
}
void ChangeInput::parseNumberedString(const QString &str, QString &prefix, int &num)
{
    int i = str.size() - 1;
    while (i >= 0 && str[i].isDigit()) --i;
    prefix = str.left(i + 1);
    bool ok;
    num = str.mid(i + 1).toInt(&ok);
    if (!ok) num = 0;
}
QStringList ChangeInput::parseRangeString(const QString &input)
{
    QStringList items;
    if (input.isEmpty()) return items;

    QStringList parts = input.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed.contains('-')) {
            QStringList range = trimmed.split('-', Qt::SkipEmptyParts);
            if (range.size() == 2) {
                QString left = range[0].trimmed();
                QString right = range[1].trimmed();
                QString prefixL, prefixR;
                int numL, numR;
                parseNumberedString(left, prefixL, numL);
                parseNumberedString(right, prefixR, numR);
                if (prefixL == prefixR && numL <= numR) {
                    int digits = left.length() - prefixL.length();
                    for (int i = numL; i <= numR; ++i) {
                        items << prefixL + QString("%1").arg(i, digits, 10, QChar('0'));
                    }
                } else {
                    items << left << right;
                }
            }
        } else {
            items << trimmed;
        }
    }
    return items;
}
