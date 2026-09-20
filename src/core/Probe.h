#pragma once
#include <QString>
#include <QJsonObject>
#include <utility>

namespace rf {

// ffprobe ko sirf ek LOCAL subprocess ki tarah call karta hai (QProcess).
// Koi network call nahi, koi URL nahi -- sirf local file path andar jaata hai.
class Probe {
public:
    static QJsonObject run(const QString& ffprobe, const QString& path, const QStringList& extra = {});
    static std::pair<int, int> probeImage(const QString& ffprobe, const QString& path);
    static double probeDuration(const QString& ffprobe, const QString& path);
};

} // namespace rf
