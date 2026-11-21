import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Updates“ (Vertrag V2, Entwurf §9): derselbe Abschnitt in allen vier Apps.
// Jeder Satz kommt aus dem Hub-Katalog (`Updates.text`), jeder Wert aus dem
// gemeinsamen Ablauf (`Updates.*`, src/viewmodels/UpdateBridge.h). Familienregel
// „wirkt sofort“: Schalter und Auswahl speichern bei jeder Änderung, Felder beim
// Verlassen oder mit Enter.
// 🔑 Die manuellen Proxy-Felder werden AUSGEBLENDET, nicht ausgegraut (Entwurf §2); ihre
// Werte bleiben beim Umschalten erhalten.
// 🔑 Ein Port außerhalb 0…65535 wird nicht gespeichert und am Feld sichtbar markiert
// (Rand in Gefahrfarbe), ohne eigenen Satz: Einen Satz für „unvollständig“ gibt
// es im gemeinsamen Katalog noch nicht.
CatPage {
    id: page
    // Sätze mit der Textrevision lesen, damit ein Sprachwechsel neu auswertet.
    function t(key, args) { return (Updates.textRevision, Updates.text(key, args || {})) }

    heading: t("set.section")
    subtitle: ""

    Connections {
        target: Updates
        // Nach Import/Zurücksetzen: Felder auch dann neu füllen, wenn sie den Fokus haben.
        function onSettingsChanged() {
            hostField.text = Updates.proxyHost
            portField.text = Updates.proxyPort > 0 ? String(Updates.proxyPort) : ""
            userField.text = Updates.proxyUser
        }
    }

    PrefAnchor {
        settingKey: "updates.check"
        page: page
        PrefGroup {
            title: page.t("set.section")
            PrefRow {
                title: page.t("set.auto")
                description: page.t("set.auto.desc")
                AppSwitch {
                    objectName: "swUpdateAutoCheck"
                    checked: Updates.autoCheck
                    onToggled: Updates.autoCheck = checked
                }
            }
            PrefRow {
                title: page.t("set.version")
                RowLayout {
                    spacing: 12
                    TextEdit {
                        text: Updates.buildId
                        readOnly: true
                        selectByMouse: true
                        color: Theme.textBright
                        font.family: App.monospaceFonts().length > 0 ? App.monospaceFonts()[0] : ""
                        font.pixelSize: 12
                    }
                    Button {
                        text: page.t("set.check")
                        palette.buttonText: Theme.textBright
                        enabled: !Updates.flow.busy
                        onClicked: Updates.openByHand()
                    }
                }
            }
            PrefRow {
                visible: Updates.skippedVersion.length > 0
                title: page.t("set.skipped")
                description: page.t("set.skipped.desc", { neu: Updates.skippedVersion })
                Button {
                    text: page.t("set.reset")
                    palette.buttonText: Theme.textBright
                    onClicked: Updates.resetSkippedVersion()
                }
            }
        }
    }

    PrefAnchor {
        settingKey: "updates.proxy"
        page: page
        PrefGroup {
            title: page.t("set.proxy.group")
            PrefRow {
                title: page.t("set.proxy")
                description: Updates.proxyMode === 1 ? page.t("set.proxy.direct.desc")
                           : Updates.proxyMode === 2 ? page.t("set.proxy.manual.desc")
                                                     : page.t("set.proxy.system.desc")
                SegmentedControl {
                    model: [page.t("set.proxy.system"), page.t("set.proxy.direct"),
                            page.t("set.proxy.manual")]
                    currentIndex: Updates.proxyMode
                    onActivated: (i) => Updates.proxyMode = i
                }
            }
            PrefRow {
                visible: Updates.proxyMode === 2
                title: page.t("set.kind")
                AppComboBox {
                    Layout.preferredWidth: 120
                    model: ["HTTP", "SOCKS5"]
                    currentIndex: Updates.proxyType
                    onActivated: Updates.proxyType = currentIndex
                }
            }
            PrefRow {
                visible: Updates.proxyMode === 2
                title: page.t("set.server")
                RowLayout {
                    spacing: 8
                    TextField {
                        id: hostField
                        Layout.preferredWidth: 200
                        color: Theme.textBright
                        text: Updates.proxyHost
                        onEditingFinished: Updates.proxyHost = text
                    }
                    TextField {
                        id: portField
                        objectName: "updatesProxyPort"
                        // Ohne Validator: Ein Wert außerhalb des Bereichs bleibt sichtbar
                        // stehen und wird markiert, statt still verworfen zu werden.
                        readonly property int value: /^[0-9]{1,5}$/.test(text) ? parseInt(text) : -1
                        readonly property bool invalid: text.length > 0 && (value < 0 || value > 65535)
                        Layout.preferredWidth: 72
                        color: Theme.textBright
                        inputMethodHints: Qt.ImhDigitsOnly
                        text: Updates.proxyPort > 0 ? String(Updates.proxyPort) : ""
                        // Wie der Basic-Standard des Server-Felds daneben (Höhe, Fläche,
                        // Rand), nur der Rand wird bei ungültigem Wert zur Gefahrfarbe.
                        background: Rectangle {
                            implicitWidth: 72
                            implicitHeight: 40
                            color: portField.palette.base
                            border.width: portField.invalid || portField.activeFocus ? 2 : 1
                            border.color: portField.invalid ? Theme.danger
                                        : portField.activeFocus ? portField.palette.highlight
                                                                : portField.palette.mid
                        }
                        onEditingFinished: {
                            if (text.length === 0) Updates.proxyPort = 0
                            else if (!invalid) Updates.proxyPort = value
                        }
                    }
                }
            }
            PrefRow {
                visible: Updates.proxyMode === 2
                title: page.t("set.user")
                description: page.t("set.user.desc")
                TextField {
                    id: userField
                    Layout.preferredWidth: 200
                    color: Theme.textBright
                    text: Updates.proxyUser
                    onEditingFinished: Updates.proxyUser = text
                }
            }
            PrefRow {
                title: page.t("set.forget")
                description: Updates.hasSessionPassword ? page.t("set.forget.some")
                                                        : page.t("set.forget.none")
                Button {
                    text: page.t("set.forget.btn")
                    enabled: Updates.hasSessionPassword
                    palette.buttonText: Theme.textBright
                    onClicked: Updates.forgetSessionPassword()
                }
            }
        }
    }
}
