![HEADER](/home/char/Desktop/Repository/GXDE/Software/gxde-control-center/docs/img/header.png)

<div align="center"> 
<a href="https://gitee.com/GXDE-OS/gxde-control-center">
<img src="./docs/img/icon.svg" alt="Logo" width="80" height="80"> 
</a>
<h3 align="center">GXDE控制中心</h3>
<p align="center"> GXDE桌面环境的控制面板 <br />
<a href="https://gxde.top">
<strong>关于GXDE »</strong></a> <br /> <br />
<a href="https://github.com/GXDE-OS/gxde-control-center/tags">查看往期版本</a>  · <a href="https://gitee.com/GXDE-OS/gxde-control-center/issues">报告Bug</a>  · <a href="https://gitee.com/GXDE-OS/gxde-control-center/issues">请求新功能</a> </p>
</div> 

## 关于本项目
GXDE Control Center 是 GXDE 桌面环境的控制面板。 
GXDE 已复刻并持续维护 GCC V4.x 版本，并将其作为GXDE的控制面板使用。请勿在Deepin系统上安装此软件包。 



### 依赖项
#### 构建时依赖
  - pkg-config
  - dpkg-dev
  - cmake (>= 3.7)
  - Qt6 (>= 6.0) 包含以下模块：
    - qt6-base-private-dev
    - libqt6svg6-dev
    - qt6-tools-dev-tools
    - qt6-multimedia-dev
  - DTK6 + DTK2 包含以下模块：
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

#### 运行时依赖
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
    
      

## 开始上手
### 编译
#### 手动编译 (命令行) 
确保您已经安装了上述依赖，并执行以下命令：
```bash
$ cd gxde-control-center
$ mkdir Build
$ cd Build
$ cmake ..
$ make
```

您可以通过以下指令安装：
```bash
$ sudo make install
```
可执行二进制文件可以在安装后在`/usr/bin/gxde-control-center`找到，插件将位于 `/usr/lib/gxde-control-center/modules/`.



### 使用
执行gxde-control-center -h可以打开帮助项。



## 里程碑
- [ ] 添加Wayland支持

  

## 参与开发
请参阅「[CONTRIBUTING](./CONTRIBUTING.md)」文件，了解贡献所需的信息。



### GXDE Control Center的贡献者们
*（**注**：原版深度控制中心的贡献者信息可以在[这里](https://github.com/linuxdeepin/dde-control-center/graphs/contributors)找到）* 
<a href="https://gitee.com/GXDE-OS/gxde-control-center/contributors">
<img src="https://contrib.rocks/image?repo=GXDE-OS/gxde-control-center" alt="contrib.rocks image" /> 
</a> 



## 许可证
该项目使用开源协议GPL-3.0，详见「[LICENSE](./LICENSE)」。



## 鸣谢
感谢以下代码与模板提供参考: 
* **Deepin Control Center**: https://github.com/linuxdeepin/dde-control-center
* **Best Readme Template**: https://github.com/othneildrew/Best-README-Template