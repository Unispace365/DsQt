# DsQt

## Description (the what and the why)

DsQt (Dee Ess Cute) is a framework for interactive applications built on top of the [Qt framework](https://qt.io/)

DsQt is a fully featured windows framework for rapid production of multimedia applications. 
It is a replacement for DS Cinder. It Relies on Qt and QML for most features  (Multimedia, Fonts, 3D rendering)
but adds comprensive configuration data modeling and integration with our 
CMS backend for rapid prototyping and developemnt.

## Documentation

Documentation is compiled by Doxygen and can be found here:
<https://unispace365.github.io/DsQt/>

## Getting Started

> This version of DsQt has been built and is targeted at Windows. We hope to have Linux and Apple builds. 

### Windows

#### Install Visual Studio 2022 - Community Edition

1. Download here: <https://www.visualstudio.com/>
2. Install it!
        - Make sure to select C++ development and ensure you have the latest windows SDK and visual studio updates!

#### Install Qt 6.9

1. Create a Qt.io account if you don't have one. <https://login.qt.io/register>
2. Go to <https://www.qt.io/download-qt-installer-oss> and download Qt for x64
3. Run the online installer.
4. You don't need to install everything. In fact I'd strongly recommend against it. But also it's hard to know what your going to need. I think the following are the important ones:

    * Extensions:
        * Qt PDF:
            * Qt 6.9.3:
                * MSVC 2022 x64
        * Qt WebEngine:
            * Qt 6.9.3:
                * MSVC 2022 x64
    * Qt:
        * Qt 6.9.3:
            * MSVC 2022 64-bit
            * Additional Libraries:
                * Qt Multimedia
                * Qt Image Formats
                * Qt Language Server
                * Qt Virtual Keyboard
                * Qt Quick 3d
                * Qt Quick Effect Maker
                * (Everything else you can get just to be safe)
    * Qt Creator (check all boxes)
5. Depending on what you selected this can take a while to download (hours sometimes).

#### Download and compile DsQt

1. Download and install Git and ensure you have git-lfs available
2. Run `git clone https://github.com/Unispace365/DsQt.git`
3. `cd DsQt`
4. Run: `git lfs install`
5. Run: `git lfs fetch --all`
6. Add platform environment variable
    1. Open the Start menu and type "Advanced System Settings"
    2. Click "Environment Variables"
    3. Under "Sytem variables", select "New..."
    4. Variable name: `DS_QT_PLATFORM_100`
    5. Variable value: the path of this downloaded repo (e.g C:\Users\YourNameHere\Documents\DsQt)
    6. Ok, Ok, Ok

#### Compile the Project Cloner

1. Open QtCreator and navigate to the CMakeLists.txt file in _DsQt_\tools\ProjectCloner\
2. Configure the project as follows: you'll only need to do this once unless you delete the ProjectCloner from the deploy folder
    1. choose the kit "Desktop Qt6.9.0 MSVC2022 64bit" and hit configure
    2. click on the build type icon on the left (looks like a monitor) and choose Release
    3. click on the **Projects** tab on the left
    4. under the kit listing for "Desktop Qt6.9.0 MSVC2022 64bit" choose the run settings
    5. in the run settings, under deployment add an install deployment step
3. Click on the edit tab on the left.
4. Right click on the root of the project and choose Deploy from the menu.
5. Click on the Shortcut in the _DsQT_\tools folder to run the ProjectCloner.

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
