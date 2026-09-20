#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace rf {

class TextFilters {
public:
    static QStringList wrapLines(const QString& text, int maxChars, int limit = 4);
    static QString drawText(const QString& font, const QString& text, int fs, const QString& color,
                             const QString& x, const QString& y, const QString& alpha = QString(),
                             const QString& enable = QString(), bool border = true);
    static QStringList captionFilters(const QJsonObject& clip, int W, int H, const QString& font, double dur, double off = 0.0);
    static QStringList overlayFilters(const QJsonArray& overlays, int W, int H, const QString& font,
                                       const QString& emojiFont, double t0 = 0.0);
};

} // namespace rf
