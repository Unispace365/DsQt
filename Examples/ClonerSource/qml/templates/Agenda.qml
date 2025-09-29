import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtMultimedia
import Dsqt
import WhiteLabelWaffles

ContentItem {
    id: root

    property bool debug: false
    property real unitH: 1 / 3840 * parent.width
    property real unitV: 1 / 2160 * parent.height

    // Theme proxy.
    Rectangle {
        id: themeProxy
        anchors.fill: parent
        visible: false

        ShaderEffect {
            anchors.fill: parent
            property variant source: theme // Use the default shader.
        }
    }

    // Background media.
    DsMediaViewer {
        id: background
        anchors.fill: parent
        media: root.model.background_media_override
        fillMode: root.model.background_media_fitting === "Fit" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        loops: MediaPlayer.Infinite
        cropOverlay: false

        onMediaLoaded: { themeProxy.visible = true }
    }

    Column {
        id: container
        anchors.fill: parent
        anchors.margins: 0.05 * parent.height
        spacing: 0.006 * parent.height

        property string textColor: "white"

        // Allows us to measure the exact text size.
        DsTextMeasurer {
            id: textMeasurer
        }

        // Headline.
        Text {
            id: headline
            width: parent.width
            color: container.textColor
            text: root.model.headline
            font.family: helveticaNeueThin.name
            font.weight: 200
            font.pixelSize: Math.min( 140/3840 * parent.width, 140/2160 * parent.height )
            minimumPixelSize: 40
            wrapMode: Text.WordWrap
            lineHeight: 0.8
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignTop
            renderTypeQuality: Text.VeryHighRenderTypeQuality
            Rectangle {
                color: "#3000ff00"
                anchors.fill: parent
                visible: root.debug
            }
            onFontChanged: textMeasurer.fit(headline)
            onWidthChanged:  textMeasurer.fit(headline)
        }

        // Sub headline.
        Text {
            id: subheadline
            width: parent.width
            color: container.textColor
            text: root.model.subheadline
            font.family: helveticaNeueMedium.name
            font.weight: 500
            font.pixelSize: 0.25 * headline.font.pixelSize
            minimumPixelSize: 40
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignTop
            renderTypeQuality: Text.VeryHighRenderTypeQuality
            Rectangle {
                color: "#3000ff00"
                anchors.fill: parent
                visible: root.debug
            }
            onFontChanged: textMeasurer.fit(subheadline)
            onWidthChanged:  textMeasurer.fit(subheadline)
        }

        Item {
            id: scrollContainer
            width: container.width
            height: container.height - y // Remaining height.

            Flickable {
                id: scroll
                anchors.horizontalCenter: scrollContainer.horizontalCenter
                anchors.verticalCenter: scrollContainer.verticalCenter
                width: Math.min( row.width, scrollContainer.width ) // Adjust width based on content.
                height: scrollContainer.height
                contentWidth: row.width
                contentHeight: row.height
                clip: true
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: row
                    spacing: 40

                    Repeater {
                        model: root.model.children
                        delegate: agendaItem
                    }
                }
            }
        }
    }

    // Agenda item component.
    Component {
        id: agendaItem

        Rectangle {
            id: frame
            y: 0.1 * scroll.height
            width: Math.min( (container.width - 3 * row.spacing) / 4, height * 0.6 )
            height: 0.9 * scroll.height
            color: "#cc191f25"
            radius: 60

            property real fontUnit: width / 800
            property date startTime: modelData.start_time
            property date endTime: modelData.end_time

            Text {
                id: itemTime
                x: 0.1 * parent.width
                y: 0.05 * parent.height
                width: 0.8 * parent.width
                color: container.textColor
                text: Qt.formatTime(startTime, "hh:mm") + " - " + Qt.formatTime(endTime, "hh:mm")
                font.family: helveticaNeueThin.name
                font.weight: 200
                font.pixelSize: 80 * fontUnit
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                renderTypeQuality: Text.VeryHighRenderTypeQuality
                Rectangle {
                    color: "#3000ff00"
                    anchors.fill: parent
                    visible: root.debug
                }
                onFontChanged: textMeasurer.fit(itemTime)
                onWidthChanged:  textMeasurer.fit(itemTime)
            }

            Text {
                id: itemTitle
                x: 0.1 * parent.width
                anchors.top: itemTime.bottom
                anchors.topMargin: 115 * unitV
                width: 0.8 * parent.width
                color: container.textColor
                text: modelData.title // "Coffee & Connect: 'What's Inspiring You?'"
                font.family: helveticaNeueRoman.name
                font.weight: 400
                font.pixelSize: 72 * fontUnit
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                renderTypeQuality: Text.VeryHighRenderTypeQuality
                Rectangle {
                    color: "#3000ff00"
                    anchors.fill: parent
                    visible: root.debug
                }
                onFontChanged: textMeasurer.fit(itemTitle)
                onWidthChanged:  textMeasurer.fit(itemTitle)
            }

            Text {
                id: itemSubTitle
                x: 0.1 * parent.width
                anchors.top: itemTitle.bottom
                anchors.topMargin: 100 * unitV
                width: 0.8 * parent.width
                color: container.textColor
                text: modelData.subtitle
                font.family: helveticaNeueThin.name
                font.weight: 200
                font.pixelSize: 40 * fontUnit
                font.capitalization: Font.AllUppercase
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                renderTypeQuality: Text.VeryHighRenderTypeQuality
                Rectangle {
                    color: "#3000ff00"
                    anchors.fill: parent
                    visible: root.debug
                }
                onFontChanged: textMeasurer.fit(itemSubTitle)
                onWidthChanged:  textMeasurer.fit(itemSubTitle)
            }

            Text {
                id: itemCopy
                x: 0.1 * parent.width
                anchors.top: itemSubTitle.bottom
                anchors.topMargin: 40 * unitV
                anchors.bottom: frame.bottom
                anchors.bottomMargin: 40 * unitV
                width: 0.8 * parent.width
                color: container.textColor
                text: modelData.copy
                font.family: helveticaNeueThin.name
                font.weight: 200
                font.pixelSize: 36 * fontUnit
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                renderTypeQuality: Text.VeryHighRenderTypeQuality
                Rectangle {
                    color: "#3000ff00"
                    anchors.fill: parent
                    visible: root.debug
                }
                onFontChanged: textMeasurer.fit(itemCopy)
                onWidthChanged:  textMeasurer.fit(itemCopy)
            }
        }
    }
}
