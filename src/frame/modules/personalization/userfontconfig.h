/*
 * Copyright (C) 2026 CharOfString
 *
 * Author:     CharOfString <root@charofstring.cc>
 *
 * Maintainer: CharOfString <root@charofstring.cc>
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
 * ------------------------------------------------------------------------------------------------
 * Saves user font preference to Archlinux config and do a font cache refresh.
 */

#ifndef USERFONTCONFIG_H
#define USERFONTCONFIG_H

#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QXmlStreamWriter>

namespace dcc {
namespace personalization {

inline bool saveUserFont(const QString &type, const QString &family, QString *error) {
    const bool standard = type == QStringLiteral("standardfont");
    if ((!standard && type != QStringLiteral("monospacefont")) || family.trimmed().isEmpty()) {
        *error = QStringLiteral("Invalid font selection");
        return false;
    }

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/fontconfig/conf.d");
    if (!QDir().mkpath(directory)) {
        *error = QStringLiteral("Cannot create %1").arg(directory);
        return false;
    }

    // Resolve generic families before later desktop-generated alias rules.
    QSaveFile file(directory + (standard ? QStringLiteral("/00-gxde-standard-font.conf")
        : QStringLiteral("/00-gxde-monospace-font.conf")));
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD(QStringLiteral("<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">"));
    xml.writeStartElement(QStringLiteral("fontconfig"));
    xml.writeComment(QStringLiteral("Managed by GXDE Control Center"));
    xml.writeStartElement(QStringLiteral("match"));
    xml.writeAttribute(QStringLiteral("target"), QStringLiteral("pattern"));
    xml.writeStartElement(QStringLiteral("test"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("family"));
    xml.writeAttribute(QStringLiteral("qual"), QStringLiteral("any"));
    xml.writeTextElement(QStringLiteral("string"), standard ? QStringLiteral("sans-serif")
        : QStringLiteral("monospace"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("edit"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("family"));
    xml.writeAttribute(QStringLiteral("mode"), QStringLiteral("assign"));
    xml.writeAttribute(QStringLiteral("binding"), QStringLiteral("strong"));
    xml.writeTextElement(QStringLiteral("string"), family);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError() || !file.commit()) {
        *error = file.errorString();
        return false;
    }

    return true;
}

}
}

#endif // USERFONTCONFIG_H
