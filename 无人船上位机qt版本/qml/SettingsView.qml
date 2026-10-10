import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "items"
import "HudTheme.js" as Theme

Item {
    id: page
    signal back()

    readonly property int formLabelW: 220
    readonly property int formFieldW: 320
    readonly property int formSchemeW: 118
    readonly property var mqttSchemes: ["mqtt", "mqtts", "ws", "wss"]

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    component FormLabel: Text {
        color: Theme.textDim
        font.pixelSize: 16
        Layout.preferredWidth: page.formLabelW
        Layout.minimumWidth: page.formLabelW
        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
    }

    Image {
        anchors.fill: parent
        source: "qrc:/equ/background.png"
        fillMode: Image.PreserveAspectCrop
    }
    Rectangle {
        anchors.fill: parent
        color: usv.settings.theme === "night" ? "#d8081018" : "#b808141c"
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.topMargin: 36
        anchors.bottomMargin: 90
        contentWidth: width
        contentHeight: col.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        Column {
            id: col
            width: flick.width
            spacing: 18

            Text { text: t("基础设置：", "General:"); color: Theme.text; font.pixelSize: 22; font.bold: true }

            GlassPanel {
                width: parent.width
                height: genGrid.height + 36
                GridLayout {
                    id: genGrid
                    width: parent.width - 36
                    x: 18
                    y: 18
                    columns: 2
                    columnSpacing: 36
                    rowSpacing: 14

                    FormLabel { text: t("显示单位", "Units") }
                    DarkButton {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        text: usv.settings.displayUnitName
                        onClicked: usv.settings.cycleDisplayUnit()
                    }

                    FormLabel { text: t("系统语言", "Language") }
                    DarkButton {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        text: usv.settings.languageName
                        onClicked: usv.settings.cycleLanguage()
                    }

                    FormLabel { text: t("地图图层", "Map layer") }
                    DarkButton {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        text: usv.settings.mapType === "satellite"
                              ? t("卫星图", "Satellite")
                              : t("天地图", "Street map")
                        onClicked: usv.settings.cycleMapType()
                    }

                    FormLabel { text: t("地图服务地址", "Map server") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mapBaseUrl
                        onEditingFinished: usv.settings.mapBaseUrl = text
                    }

                    FormLabel { text: t("断连保护", "Disconnect protect") }
                    Switch { checked: usv.settings.dcProtOn; onToggled: usv.settings.dcProtOn = checked }

                    FormLabel { text: t("低电量保护", "Low-battery protect") }
                    Switch { checked: usv.settings.lbProtOn; onToggled: usv.settings.lbProtOn = checked }
                }
            }

            Text { text: t("通信设置：", "Communication:"); color: Theme.text; font.pixelSize: 22; font.bold: true }

            GlassPanel {
                width: parent.width
                height: commGrid.height + 36
                GridLayout {
                    id: commGrid
                    width: parent.width - 36
                    x: 18
                    y: 18
                    columns: 2
                    columnSpacing: 36
                    rowSpacing: 12

                    FormLabel { text: t("通信模式", "Link mode") }
                    DarkComboBox {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        model: ["mqtt", "udp"]
                        currentIndex: usv.settings.commMode === "udp" ? 1 : 0
                        onActivated: usv.settings.commMode = model[index]
                    }

                    FormLabel { text: "MQTT Host" }
                    RowLayout {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        spacing: 8
                        DarkComboBox {
                            Layout.preferredWidth: page.formSchemeW
                            Layout.maximumWidth: page.formSchemeW
                            Layout.fillWidth: false
                            model: ["mqtt://", "mqtts://", "ws://", "wss://"]
                            currentIndex: Math.max(0, page.mqttSchemes.indexOf(usv.settings.mqttScheme))
                            onActivated: usv.settings.mqttScheme = page.mqttSchemes[index]
                        }
                        DarkTextField {
                            Layout.fillWidth: true
                            text: usv.settings.mqttHost
                            onEditingFinished: usv.settings.mqttHost = text
                        }
                    }

                    FormLabel { text: "MQTT Port" }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: String(usv.settings.mqttPort)
                        onEditingFinished: usv.settings.mqttPort = Number(text)
                    }

                    FormLabel { text: "MQTT TLS" }
                    Switch { checked: usv.settings.mqttSsl; onToggled: usv.settings.mqttSsl = checked }

                    FormLabel { text: t("忽略证书", "Insecure TLS") }
                    Switch { checked: usv.settings.mqttSslInsecure; onToggled: usv.settings.mqttSslInsecure = checked }

                    FormLabel { text: t("用户名", "Username") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttUsername
                        onEditingFinished: usv.settings.mqttUsername = text
                    }

                    FormLabel { text: t("密码", "Password") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttPassword
                        echoMode: TextInput.Password
                        onEditingFinished: usv.settings.mqttPassword = text
                    }

                    FormLabel { text: "Client ID" }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttClientId
                        onEditingFinished: usv.settings.mqttClientId = text
                    }

                    FormLabel { text: "Boat ID" }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttBoatId
                        onEditingFinished: usv.settings.mqttBoatId = text
                    }

                    FormLabel { text: t("发送主题", "Publish topic") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttPublishTopic
                        onEditingFinished: usv.settings.mqttPublishTopic = text
                    }

                    FormLabel { text: t("遥测订阅", "Telemetry topic") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.mqttTelemetryTopic
                        onEditingFinished: usv.settings.mqttTelemetryTopic = text
                    }

                    FormLabel { text: t("北斗信号", "BDS signal") }
                    Text {
                        Layout.preferredWidth: page.formFieldW
                        color: (usv.telemetry.online && usv.telemetry.hasFix) ? Theme.ok : Theme.warn
                        font.pixelSize: 16
                        font.bold: true
                        text: {
                            if (!usv.telemetry.online)
                                return t("未连接", "Offline")
                            if (!usv.telemetry.hasFix)
                                return t("无定位", "No fix")
                            return t("已连接", "Online")
                        }
                    }

                    FormLabel { text: t("UDP 本机地址", "UDP local host") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.udpLocalHost
                        onEditingFinished: usv.settings.udpLocalHost = text
                    }

                    FormLabel { text: t("UDP 本机端口", "UDP local port") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: String(usv.settings.udpLocalPort)
                        onEditingFinished: usv.settings.udpLocalPort = Number(text)
                    }

                    FormLabel { text: t("UDP 船端地址", "UDP remote host") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.udpRemoteHost
                        onEditingFinished: usv.settings.udpRemoteHost = text
                    }

                    FormLabel { text: t("UDP 船端端口", "UDP remote port") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: String(usv.settings.udpRemotePort)
                        onEditingFinished: usv.settings.udpRemotePort = Number(text)
                    }
                }
            }

            Row {
                spacing: 16
                DarkButton {
                    text: t("应用通信设置并重连", "Apply & reconnect")
                    onClicked: usv.applyLinkSettings()
                }
                DarkButton {
                    text: t("强制退出低电保护", "Force exit low-battery")
                    onClicked: usv.control.forceExitLowBattery()
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.WrapAnywhere
                text: t("数据目录", "Data folder") + ": " + usv.dataDir
                color: Theme.textMute
                font.pixelSize: 13
            }
            Text {
                width: parent.width
                wrapMode: Text.WrapAnywhere
                text: t("运行日志", "Log") + ": " + usv.logPath
                color: Theme.textMute
                font.pixelSize: 13
            }
            Text {
                width: parent.width
                wrapMode: Text.WrapAnywhere
                text: t("航行日志", "Voyage log") + ": " + usv.voyageLogPath
                color: Theme.textMute
                font.pixelSize: 13
            }

            Text { text: t("预警设置：", "Alert:"); color: Theme.text; font.pixelSize: 22; font.bold: true }

            GlassPanel {
                width: parent.width
                height: alertGrid.height + 36
                GridLayout {
                    id: alertGrid
                    width: parent.width - 36
                    x: 18
                    y: 18
                    columns: 2
                    columnSpacing: 36
                    rowSpacing: 12
                    FormLabel { text: t("DCPA阈值", "DCPA") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.dcpa
                        onEditingFinished: usv.settings.dcpa = text
                    }
                    FormLabel { text: t("TCPA阈值", "TCPA") }
                    DarkTextField {
                        Layout.preferredWidth: page.formFieldW
                        Layout.maximumWidth: page.formFieldW
                        Layout.fillWidth: false
                        text: usv.settings.tcpa
                        onEditingFinished: usv.settings.tcpa = text
                    }
                    FormLabel { text: t("报警语音", "Alert voice") }
                    Switch { checked: usv.settings.alertVoice; onToggled: usv.settings.alertVoice = checked }
                    FormLabel { text: t("界面闪烁", "Flash") }
                    Switch { checked: usv.settings.flash; onToggled: usv.settings.flash = checked }
                    FormLabel { text: t("AIS标签", "AIS label") }
                    Switch { checked: usv.settings.aisLabel; onToggled: usv.settings.aisLabel = checked }
                }
            }
        }
    }

    GlassCard {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 24
        width: 220
        height: 64
        title: t("返回主页", "Back")
        subtitle: t("回到驾驶界面", "Return to HUD")
        icon: "back"
        showChevron: false
        onClicked: page.back()
    }
}
