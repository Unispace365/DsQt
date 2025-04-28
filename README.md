# DsQt

## Description (the what and the why)
DsQt (Dee Ess Cute) is a framework for interactive applications built on top of the [Qt framework](https://qt.io/)

DsQt is a fully featured windows framework for rapid production of multimedia applications. 
It is a replacement for DS Cinder. It Relies on Qt and QML for most features  (Multimedia, Fonts, 3D rendering)
but adds comprensive configuration data modeling and integration with our 
CMS backend for rapid prototyping and developemnt.

## Getting Started
> This version of DsQt has been built and is targeted at Windows. We hope to have Linux and Apple builds. 

### Windows
**Install Visual Studio 2022 - Community Edition**
1. Download here: https://www.visualstudio.com/
2. Install it!
        - Make sure to select C++ development and ensure you have the latest windows SDK and visual studio updates!

#### Install Qt 6.9
1. Create a Qt.io account if you don't have one. (https://login.qt.io/register)
1. Go to https://www.qt.io/download-qt-installer-oss and download Qt for x64
2. Run the online installer.
3. You don't need to install everything. In fact I'd strongly recommend against it. But also it's hard to know what your going to need. I the following are the importand ones:
    * Qt->Qt 6.9.0->MSVC 2022 64-bit
    * Qt->Qt 6.9.0->Additional Libraries->
        * Qt Multimedia
        * Qt Image Formats
        * Qt Language Server
        * Qt Virtual Keyboard
        * Qt Quick 3d
        * Qt Quick Effect Maker
        * (Everything else you can get just to be safe)
    * Qt Creator (check all boxes)
1. Depending on what you selected this can take a while to download. (hours sometimes)

#### Download and compile DsQt
1. Download and install Git and ensure you have git-lfs available
2. Run `git clone https://github.com/Unispace365/DsQt.git`
3. `cd DsQt`
4. Run: `git lfs install`
5. Run: `git lfs fetch --all`

7. Add platform environment variable
    1. Open the Start menu and type "Advanced System Settings"
    2. Click "Environment Variables"
    3. Under "Sytem variables", select "New..."
    4. Variable name: `DS_QT_PLATFORM_100`
    5. Variable value: the path of this downloaded repo (e.g C:\Users\YourNameHere\Documents\DsQt)
    6. Ok, Ok, Ok
8. Open QtCreator and copy the Examples/DsqtApp folder to another directory that will become your project directory.
9. Open The copied CMakeLists.txt using QtCreator.

## Getting to know DsQt

**This is here as a place holder for better documentation to come.**

1. Check out the [Getting Started Example](/example/getting_started/) - it's a handy quick intro for basic concepts in ds_cinder. It also has a good number of common ds_cinder components in the app so you can mess with the pieces and see what's what. 
2. Read the docs in [doc/basics](/doc/basics/) Particularly good ones:
        1. [App Structure](/doc/basics/App%20Structure.md)
        2. [Settings](/doc/basics/Settings.md)
        3. [Dynamic Interfaces](/doc/basics/Dynamic%20Interfaces.md)
        4. [Fonts](/doc/basics/Fonts.md)
        5. [Keyboard Shortcuts](/doc/basics/Keyboard%20Shortcuts.md)
        6. [Installers](/doc/basics/Installers.md)

## Generating documentation
DsQt is documented using the doxygen syntax. Need to add doxygen to CmakeFiles.

## Starting a project
We need to write a project cloner for DsQt

### Linux + Mac
????


