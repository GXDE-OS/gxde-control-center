#ifndef FONTFAMILIES_H
#define FONTFAMILIES_H

#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QStringList>

namespace dcc {
namespace personalization {

inline QList<QJsonObject> parseFontFamilies(const QByteArray &output, const QLocale &locale)
{
    QList<QJsonObject> fonts;
    QSet<QString> seen;
    const QString language = locale.name().replace('_', '-').toLower();
    for (const QString &line : QString::fromUtf8(output).split('\n')) {
        const QStringList fields = line.split('\t');
        if (fields.size() != 2)
            continue;
        const QStringList families = fields[0].split(',');
        const QStringList languages = fields[1].trimmed().split(',');
        // Appearance validates canonical family names, not localized aliases.
        const int english = languages.indexOf(QStringLiteral("en"));
        const QString id = families.value(english >= 0 ? english : 0).trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        const int localized = languages.indexOf(language);
        const QString name = localized >= 0 ? families.value(localized, id).trimmed() : id;
        fonts.append(QJsonObject{{"Id", id}, {"Name", name},
                                 {"Aliases", fields[0]}, {"type", "standardfont"}});
    }
    return fonts;
}

}
}
#endif
