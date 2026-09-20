#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace rf {

/// Projects sirf local .json files hain (AppPaths::projects() folder me).
/// Koi database, koi server, koi network involved nahi.
class ProjectStore {
public:
    static QString projectName(const QString& raw, const QString& fallback = "project");
    static QJsonObject projectRow(const QString& path);
    static QJsonArray listProjects();
    static QJsonObject blankProject();
};

} // namespace rf
