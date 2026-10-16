import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import ProjectCloner

ColumnLayout {
    id: root
    required property Cloner cloner
    readonly property PackageCatalog catalog: cloner.catalog
    spacing: 18
    GroupBox {
        title: qsTr("1. DsQt package")
        Layout.fillWidth: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    Layout.preferredWidth: 240
                    model: [qsTr("Installed library"), qsTr("Local vcpkg package"), qsTr("Published vcpkg package")]
                    readonly property list<string> keys: ["installed", "local", "registry"]
                    currentIndex: keys.indexOf(root.catalog.source)
                    enabled: !root.catalog.busy
                    Accessible.name: qsTr("DsQt package source")
                    onActivated: root.catalog.source = keys[currentIndex]
                }
                Label { Layout.fillWidth: true; text: qsTr("Choose where this application gets DsQt."); wrapMode: Text.WordWrap }
            }
            RowLayout {
                visible: root.catalog.source !== "registry"
                Layout.fillWidth: true
                TextField {
                    id: packagePath
                    Layout.fillWidth: true
                    text: root.catalog.packagePath
                    placeholderText: root.catalog.source === "local" ? qsTr("Folder containing dsqt-local-package.json") : qsTr("DsQt installation folder")
                    Accessible.name: qsTr("Package folder")
                    onEditingFinished: root.catalog.packagePath = text
                }
                Button { text: qsTr("Browse…"); onClicked: packageFolder.open() }
                Button { text: qsTr("Check"); onClicked: { root.catalog.packagePath = packagePath.text; root.catalog.inspectPath(); } }
            }
            RowLayout {
                visible: root.catalog.source === "registry"
                Layout.fillWidth: true
                ComboBox {
                    Layout.fillWidth: true
                    model: root.catalog.packages
                    textRole: "label"
                    currentIndex: root.catalog.packageIndex
                    enabled: !root.catalog.busy
                    Accessible.name: qsTr("Published DsQt version")
                    onActivated: root.catalog.packageIndex = currentIndex
                }
                Button { text: root.catalog.busy ? qsTr("Loading…") : qsTr("Refresh"); enabled: !root.catalog.busy; onClicked: root.catalog.refreshRegistry() }
            }
            Label { Layout.fillWidth: true; text: root.catalog.statusMessage; wrapMode: Text.WordWrap; color: root.catalog.ready ? "#287346" : "#916015" }
        }
    }
    GroupBox {
        title: qsTr("2. Application modules")
        Layout.fillWidth: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: qsTr("Start with Core. Add only what your application needs."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            GridLayout {
                columns: root.width > 730 ? 2 : 1
                columnSpacing: 24
                rowSpacing: 8
                Layout.fillWidth: true
                Repeater {
                    model: root.catalog.modules
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 330
                        spacing: 0
                        CheckBox { text: modelData.label; checked: modelData.selected; enabled: !modelData.locked && !root.catalog.busy; onToggled: root.catalog.setModule(modelData.name, checked) }
                        Label { Layout.leftMargin: 32; Layout.fillWidth: true; text: modelData.detail; wrapMode: Text.WordWrap; font.pixelSize: 12; opacity: 0.8 }
                    }
                }
            }
        }
    }
    GroupBox {
        title: qsTr("3. Qt requirements")
        Layout.fillWidth: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Label { Layout.fillWidth: true; text: root.catalog.requirements; wrapMode: Text.WordWrap; lineHeight: 1.2 }
            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: 14
                Repeater {
                    model: root.cloner.detectedQtVersions
                    delegate: CheckBox {
                        required property string modelData
                        readonly property bool compatible: {
                            const requirements = root.catalog.requirements;
                            return root.cloner.isQtCompatible(modelData);
                        }
                        text: compatible ? qsTr("Qt %1").arg(modelData) : qsTr("Qt %1 — incompatible").arg(modelData)
                        enabled: compatible && !root.catalog.busy
                        checked: compatible && root.cloner.selectedQtVersions.indexOf(modelData) >= 0
                        onToggled: {
                            let versions = root.cloner.selectedQtVersions.slice();
                            const index = versions.indexOf(modelData);
                            if (checked && index < 0) versions.push(modelData);
                            if (!checked && index >= 0) versions.splice(index, 1);
                            root.cloner.selectedQtVersions = versions;
                        }
                    }
                }
            }
            Label { visible: root.cloner.detectedQtVersions.length === 0; text: qsTr("No MSVC x64 Qt kits found. Install a compatible kit, then rescan."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Button { text: qsTr("Rescan Qt kits"); onClicked: root.cloner.detectQtVersions() }
        }
    }
    FolderDialog {
        id: packageFolder
        title: qsTr("Choose the DsQt package folder")
        onAccepted: root.catalog.packagePath = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, "").replace(/^\/(?=[A-Za-z]:)/, ""))
    }
}
