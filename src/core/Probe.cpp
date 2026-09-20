#include "Probe.h"
#include "Json.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>

namespace rf {

QJsonObject Probe::run(const QString& ffprobe, const QString& path, const QStringList& extra) {
    QStringList args = { "-v", "quiet", "-print_format", "json", "-show_streams", "-show_format" };
    args += extra;
    args << path;

    QProcess p;
    p.start(ffprobe, args);
    if (!p.waitForStarted(5000)) return {};
    p.waitForFinished(30000);
    QByteArray out = p.readAllStandardOutput();
    if (out.isEmpty()) return {};
    auto doc = QJsonDocument::fromJson(out);
    return doc.isObject() ? doc.object() : QJsonObject();
}

std::pair<int, int> Probe::probeImage(const QString& ffprobe, const QString& path) {
    QJsonObject d = run(ffprobe, path);
    QJsonArray streams = d.value("streams").toArray();
    for (const auto& sv : streams) {
        QJsonObject s = sv.toObject();
        if (s.contains("width"))
            return { (int)J::num(s, "width", 1920), (int)J::num(s, "height", 1080) };
    }
    return { 1920, 1080 };
}

double Probe::probeDuration(const QString& ffprobe, const QString& path) {
    QJsonObject d = run(ffprobe, path);
    QJsonObject fmt = d.value("format").toObject();
    return J::num(fmt, "duration", 0.0);
}

} // namespace rf
