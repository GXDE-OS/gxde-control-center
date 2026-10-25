/*
 * Copyright (C) 2026 GXDE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SESSIONENVIRONMENT_H
#define SESSIONENVIRONMENT_H

#include <QProcessEnvironment>

namespace dcc {

// dtk 的 dxcb 插件初始化时会在进程内改写环境变量（清空 QT_QPA_PLATFORM、设置
// DXCB_FAKE_PLATFORM_NAME_XCB 等），且会经 QProcess 遗传给外部子进程，导致
// dtk 程序（如 gxde-wallpaper-chooser）以错误的平台模式初始化、窗口装饰异常。
// 必须在 main 入口、任何 dtk 初始化之前先调用一次本函数保存会话原始环境，
// 之后启动外部程序时使用它替代 systemEnvironment()。
inline const QProcessEnvironment &sessionEnvironment()
{
    static const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env;
}

}

#endif  // SESSIONENVIRONMENT_H
