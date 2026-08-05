import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {           // 使用ApplicationWindow替代Item作为根窗口
    width: 400
    height: 300
    visible: true
    title: "我的QML应用"

    Column {
        spacing: 20
        anchors.centerIn: parent

        Text {
            id: displayText
            text: "Hello QML"
            font.pixelSize: 24
        }

        Button {
            text: "点击我"
            onClicked: {
                displayText.text = "按钮被点击了！"
            }
        }

        TextField {
            id: inputField
            placeholderText: "输入你的名字"
            width: 200
        }

        Button {
            text: "提交"
            onClicked: {
                displayText.text = "你好，" + inputField.text + "！"
            }
        }
    }
}
