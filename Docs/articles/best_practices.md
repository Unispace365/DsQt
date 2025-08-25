\page best_practices QML Best Practices

[TOC]

This document describes things we learned while authoring QML user interfaces. It's a work-in-progress.

# Use Columns and Rows

When laying out items in a column or row, use the appropriate `Column` and `Row` elements. This makes it much easier to adjust spacing later on. It also does not require you to calculate element positions by hand, which can become complicated rather fast if you have multiple elements you need to lay out.

Columns and rows also work with `Repeater` elements. The repeater places each item as a child of the `Column` or `Row` element it is wrapped inside of. Here's an example:

```qml
DsEventSchedule {
    id: contentEvents
    type: "Scheduled Content"
}

DsEventSchedule {
    id: pinboardEvents
    type: "Pinboard Event"
}

Rectangle {
    id: schedule
    anchors.fill: parent
    anchors.margins: 20

    Column {
        spacing: 50

        Row {
            Repeater {
                model: pinboardEvents.timeline

                Rectangle {
                    x: (modelData.secondsSinceMidnight / 86400.0) * schedule.width
                    width: (modelData.durationInSeconds / 86400.0) * schedule.width
                    height: 30
                    radius: 15
                }
            }
        }

        Row {
            Repeater {
                model: contentEvents.timeline                

                Rectangle {
                    x: (modelData.secondsSinceMidnight / 86400.0) * schedule.width
                    width: (modelData.durationInSeconds / 86400.0) * schedule.width
                    height: 30
                    radius: 15
                }
            }
        }

        Column {
            spacing: 5
            
            Repeater {
                model: contentEvents.events

                Rectangle {
                    x: (modelData.secondsSinceMidnight / 86400.0) * schedule.width
                    width: (modelData.durationInSeconds / 86400.0) * schedule.width
                    height: 30
                    radius: 15
                }
            }
        }
    }
}
```

# Use Relative Layout

Try not to use absolute coordinates when positioning elements. It's way better and more flexible to use QML's `anchors`. It allows you to specify positions relative to other elements, including spacing between them. Here's an example:

```qml
Rectangle {
    id: background
    color: "black"

    anchors.fill: parent // Make it as large as its parent, usually the Window in this case.
}

Rectangle {
    id: photo
    width: 256
    height: 256
    color: "red"

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.margins: 64
}

Rectangle {
    id: text
    width: 768
    height: 1024
    color: "blue"

    anchors.top: parent.top
    anchors.left: photo.right
    anchors.margins: 64
}
```

# Use Percentages Instead of Pixels

To keep your layout dynamic and allow it to adjust to different resolutions, stop thinking in pixels! Instead, specify your layout using percentages. In CSS, this is done using [Viewport Units](https://www.sitepoint.com/css-viewport-units-quick-start/), but in QML you can do it as follows:

```qml
Rectangle {
    id: test
    color: "red"

    anchors.fill: parent
    //anchors.margins: 0.15 * Window.width // = 15vw
    //anchors.margins: 0.15 * Window.height // = 15vh
    anchors.margins: 0.15 * Math.min( Window.width, Window.height ) // = 15vmin
    //anchors.margins: 0.15 * Math.max( Window.width, Window.height ) // = 15vmax
}
```

# Scaling Text to Fit

Some templates require to scale text up or down to make the best use of the available space. In QML, this is how you can achieve it:

```qml
Rectangle {
    id: box
    anchors.fill: parent
    anchors.margins: 0.05 * Math.min( Window.width, Window.height )
    color: "#30000000"
}

Text {
    id: title
    anchors.fill: box

    color: "white"

    text: "From all his policies and webs of fear and treachery, from all his stratagems and wars his mind shook free; and throughout his realm a tremor ran, his slaves quailed, and his armies halted, and his captains suddenly steerless, bereft of will, wavered and despaired. For they were forgotten. The whole mind and purpose of the Power that wielded them was now bent with overwhelming force upon the Mountain. At his summons, wheeling with a rending cry, in a last desperate race there flew, faster than the winds, the Nazgûl, the Ringwraiths, and with a storm of wings they hurtled southwards to Mount Doom."
    
    // lineHeight: 0.8  // Note that line heights other than 1 may not work well with vertical alignment.
    wrapMode: Text.WordWrap
    horizontalAlignment: Text.AlignJustify
    verticalAlignment: Text.AlignVCenter

    fontSizeMode: Text.Fit
    font.family: "Calisto MT"
    font.pixelSize: 1000 // Start with a large font size and scale it down automatically.
    minimumPixelSize: 1  // Optional: ensure it scales down as much as needed.
    renderTypeQuality: Text.VeryHighRenderTypeQuality // or use: Text.NativeRendering.
}
```
