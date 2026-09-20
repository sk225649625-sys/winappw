#include "ProjectStore.h"
#include "AppPaths.h"
#include "MediaStore.h"
#include "../core/Json.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <algorithm>

namespace rf {

QString ProjectStore::projectName(const QString& raw, const QString& fallback) {
    QString base = QFileInfo(raw.trimmed()).fileName();
    QString name = MediaStore::safeName(base).trimmed();
    if (name.isEmpty()) name = fallback;
    if (name.endsWith(".json", Qt::CaseInsensitive)) name.chop(5);
    while (!name.isEmpty() && (name.startsWith('.') || name.startsWith('_') || name.startsWith(' ')))
        name.remove(0, 1);
    while (!name.isEmpty() && (name.endsWith('.') || name.endsWith('_') || name.endsWith(' ')))
        name.chop(1);
    if (name.isEmpty()) name = fallback;
    if (name.length() > 60) name = name.left(60);
    return name + ".json";
}

QJsonObject ProjectStore::projectRow(const QString& path) {
    QJsonObject row;
    QString name = QFileInfo(path).fileName();
    row["name"] = name; row["mtime"] = 0; row["size"] = 0;
    row["clips"] = 0; row["voice"] = 0; row["audio"] = 0; row["dur"] = 0.0; row["ok"] = false;

    QFileInfo fi(path);
    if (!fi.exists()) return row;
    row["mtime"] = (double)fi.lastModified().toSecsSinceEpoch();
    row["size"] = fi.size();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return row;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return row;
    QJsonObject d = doc.object();
    QJsonArray clips = J::arr(d, "clips");
    QJsonValue voiceNode = d.value("voice");
    int voiceCount = voiceNode.isObject() ? 1 : (voiceNode.isArray() ? voiceNode.toArray().size() : 0);
    double dur = 0;
    for (const auto& c : clips) dur += J::num(c.toObject(), "dur", 0);

    row["clips"] = clips.size();
    row["voice"] = voiceCount;
    row["audio"] = J::arr(d, "audio").size();
    row["dur"] = std::round(dur * 100) / 100.0;
    row["ratio"] = J::str(d, "ratio", "16:9");
    row["ok"] = true;
    return row;
}

QJsonArray ProjectStore::listProjects() {
    QDir dir(AppPaths::projects());
    QList<QJsonObject> rows;
    if (dir.exists())
        for (const auto& name : dir.entryList({ "*.json" }, QDir::Files))
            rows << projectRow(dir.filePath(name));
    std::sort(rows.begin(), rows.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return J::num(a, "mtime") > J::num(b, "mtime");
    });
    QJsonArray arr;
    for (auto& r : rows) arr.append(r);
    return arr;
}

QJsonObject ProjectStore::blankProject() {
    QJsonObject p;
    p["ratio"] = "16:9"; p["quality"] = 1080; p["fps"] = 30;
    p["clips"] = QJsonArray(); p["audio"] = QJsonArray(); p["voice"] = QJsonArray();
    p["overlays"] = QJsonArray(); p["fxg"] = QJsonObject(); p["afx"] = QJsonObject();
    return p;
}

} // namespace rf
