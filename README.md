![HEADER](./docs/img/header.png)
<div align="center"> <a href="https://gitee.com/GXDE-OS/gxde-control-center"> <img src="./docs/img/icon.png" alt="Logo" width="80" height="80"> </a>

<h3 align="center">GXDE Control Center</h3>

<p align="center"> The control panel for the GXDE Desktop Environment <br /> <a href="https://www.gxde.top/en/"><strong>About GXDE »</strong></a> <br /> <br /> <a href="https://github.com/GXDE-OS/gxde-control-center/tags">View Previous Releases</a> &middot; <a href="https://gitee.com/GXDE-OS/gxde-control-center/issues">Report a Bug</a> &middot; <a href="https://gitee.com/GXDE-OS/gxde-control-center/issues">Request a Feature</a> </p> </div>

  ## About the Project

GXDE Control Center is the control panel of GXDE Desktop Environment.

GXDE has forked and maintained GCC V4.x and used as control panel of GXDE. You should NOT install this package on Deepin.

  ### Dependencies

  #### Build Dependencies

  - pkg-config
  - dpkg-dev
  - cmake (>= 3.7)
  - Qt6 (>= 6.0) with the following modules:
    - qt6-base-private-dev
    - libqt6svg6-dev
    - qt6-tools-dev-tools
    - qt6-multimedia-dev
  - DTK6 + DTK2 with the following modules:
    - libdtk6core-dev
    - libdtk2widget6-dev
    - libdtk6core-bin
  - libdframeworkdbus-qt6-dev
  - libgsettings-qt6-dev
  - libgxde-network-utils-qt6-dev (>= 1.0.0)
  - libkf6networkmanagerqt-dev
  - libx11-dev
  - libxrandr-dev
  - libmtdev-dev
  - libfontconfig1-dev
  - libfreetype6-dev
  - libegl1-mesa-dev
  - libxcb-ewmh-dev
  - libwayland-dev
  - liblayershellqtinterface-dev

  #### Runtime Dependencies

  - Qt6 (>= 6.0)
    - Qt6-DBus
    - Qt6-Multimedia
    - Qt6-MultimediaWidgets
    - Qt6-Svg
  - libdtk6core
  - libdtk2widget6
  - libdframeworkdbus-qt6
  - libkf6networkmanagerqt6
  - libgxde-network-utils-qt6
  - libgsettings-qt6
  - [gxde-api](https://github.com/GXDE-OS/gxde-api)
  - [deepin-daemon](https://github.com/linuxdeepin/dde-daemon)
  - [startdde](https://github.com/linuxdeepin/startdde)
  - geoip-database

  ## Getting Started

  ### Building

  #### Manual Build (Command Line)

  Make sure that all the dependencies listed above are installed, and then run the following commands:

  ```bash
  $ cd gxde-control-center
  $ mkdir Build
  $ cd Build
  $ cmake ..
  $ make
  ```

  Install the application with the following command:

  ```bash
  $ sudo make install
  ```

  After installation, the executable binary can be found at
  `/usr/bin/gxde-control-center`, and the plugins will be installed in
  `/usr/lib/gxde-control-center/modules/`.


  #### Manual Build (DEB build script)

Building script may be found at [./build-deb](./build-deb), this is a shell script that helps you generate the `.deb` package while you debug to install on test machine.



First fix the permission issue:

```bash
$ chmod a+x ./build-deb
```



Then you may learn the script's parameter:

```bash
./build-deb <Options>

Options:
  -b, --binary            Build binary package only (default)
    -d, --install-deps    Build after installing dependencies
    -c, --clean           Clear artifacts
    -h, --help            Print help info
```



It is recommend to run this command for your first build:

```bash
$ ./build-deb -d    # Auto-install dependencies and build
```


From now on, you'll no longer need to install any dependency for this package:

```bash
$ ./build-deb    # Build directly
```


Do a cleanup as you're done: 

```bash
$ ./build-deb -c
```
You should also note that the artifacts will also be cleared.


  ### Usage

  Run `gxde-control-center -h` to display the available help options.

  ## Milestones

  -  Add Wayland support

  ## Contributing

  Please refer to the [CONTRIBUTING](CONTRIBUTING.md) file for information about contributing to this project.

  ### GXDE Control Center Contributors

  *(**Note:** Contributor information for the original Deepin Control Center can be found [here](https://github.com/linuxdeepin/dde-control-center/graphs/contributors).)*

  ## License

  This project is licensed under the `GPL-3.0` license. See
  [LICENSE](https://chatgpt.com/c/LICENSE) for details.

  ## Acknowledgements

  Thanks to the following projects for providing code and template references:

  - **Deepin Control Center**: https://github.com/linuxdeepin/dde-control-center
  - **Best README Template**: https://github.com/othneildrew/Best-README-Template
