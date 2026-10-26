在个性化主页「动态壁纸」一栏上方新增「壁纸与屏保」入口行，点击后唤起外部程序 `gxde-wallpaper-chooser`（不实现页面内容）：

1. **personalizationmodel.h/.cpp**：新增 `isInstallWallpaperChooser()`，检测 `/usr/bin/gxde-wallpaper-chooser` 是否存在（与现有 `isInstallVideoWallpaper` 的 `QFile::exists` 模式一致，未安装则不显示该行）
2. **personalizationwidget.h**：新增信号 `showWallpaperChooserWidget()`
3. **personalizationwidget.cpp**：构造函数中在「字体」之后、「动态壁纸」之前插入 `NextPageWidget`，标题 `tr("Wallpaper and Screensaver")`，仅已安装时 `appendItem`，点击发信号
4. **personalizationmodule.h/.cpp**：连接信号到新槽 `showWallpaperChooser()`，实现为 `QProcess::startDetached("gxde-wallpaper-chooser")`，启动失败打 qWarning
5. **翻译**：跑 `translate_src2ts.sh`（lupdate）把新字符串写进 .ts，在 zh_CN 补「壁纸与屏保」
6. 编译验证 + commit（`feat(personalization): ...`）

无新增源文件，不需要改 CMakeLists。注意：本机尚未安装 `gxde-wallpaper-chooser` 二进制，运行验证时该行会因未安装而隐藏，装上后（你的 chooser 项目 deb）即显示。