/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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

#ifndef GXDECURSOR_H
#define GXDECURSOR_H

#include <QString>

namespace GxdeCursor {

// Wayland 下 QCursor::pos() 只能拿到本窗口内的局部坐标，控制中心隐藏时更是无从
// 得知指针位置，因此通过 ukui_shell 直接向合成器询问指针所在的输出。
// 返回空字符串表示合成器不支持或查询失败。
QString currentOutputName();

} // namespace GxdeCursor

#endif // GXDECURSOR_H
