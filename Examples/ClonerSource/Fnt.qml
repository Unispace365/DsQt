pragma Singleton
import QtQuick
import Dsqt

Item {

    //A Default font can be set in engine.font.toml and is by default load with the main engine.toml

    //we can alias out any font loaders, but also see engine.font.toml for font loading.
    readonly property alias bitcount : bitcountPropSingle_var

    // Load fonts.
    FontLoader {
        id: bitcountPropSingle_var
        source: Ds.env.expand("file://%APP%/data/fonts/bitcount/BitcountPropSingle-VariableFont_CRSV,ELSH,ELXP,slnt,wght.ttf")
    }

    //font definitions
    readonly property font bitcountClk: ({ family: bitcountPropSingle_var.name, pixelSize: 20, weight: 450 })
    readonly property font main: ({ family: "Inter", pixelSize: 40, weight: 450 })
    readonly property font menuTitle: ({ family: "Inter", pixelSize: 40, weight: 700 })

}
