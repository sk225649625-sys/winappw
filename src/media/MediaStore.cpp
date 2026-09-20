#include "MediaStore.h"
#include "AppPaths.h"
#include "../core/Json.h"
#include "../core/Probe.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QDateTime>

namespace rf {

static const QRegularExpression kImgExt("\\.(jpe?g|jfif|png|webp|gif|bmp|avif|tiff?)$", QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression kAudExt("\\.(mp3|wav|m4a|aac|ogg|oga|flac|opus|wma)$", QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression kSafe("[^A-Za-z0-9._ \\-()\\[\\]]+");

MediaStore::MediaStore(QString ffmpeg, QString ffprobe)
    : m_ffmpeg(std::move(ffmpeg)), m_ffprobe(std::move(ffprobe)) {
    m_kindsFile = QDir(AppPaths::media()).filePath(".kinds.json");
}

bool MediaStore::isImage(const QString& name) { return kImgExt.match(name).hasMatch(); }
bool MediaStore::isAudio(const QString& name) { return kAudExt.match(name).hasMatch(); }
QString MediaStore::safeName(const QString& name) { QString n = name; return n.replace(kSafe, "_"); }

QMap<QString, QString> MediaStore::loadKinds() {
    QMap<QString, QString> d;
    QFile f(m_kindsFile);
    if (!f.open(QIODevice::ReadOnly)) return d;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject())
        for (auto it = doc.object().begin(); it != doc.object().end(); ++it)
            d[it.key()] = it.value().toString();
    return d;
}

void MediaStore::saveKinds(const QMap<QString, QString>& d) {
    QJsonObject o;
    for (auto it = d.begin(); it != d.end(); ++it) o[it.key()] = it.value();
    QFile f(m_kindsFile);
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void MediaStore::setKind(const QString& name, const QString& kind) {
    auto d = loadKinds(); d[name] = kind; saveKinds(d);
}
void MediaStore::dropKind(const QString& name) {
    auto d = loadKinds(); d.remove(name); saveKinds(d);
}
QString MediaStore::getKind(const QString& name, const QString& def) {
    auto d = loadKinds(); return d.value(name, def);
}

qint64 MediaStore::nameDate(const QString& name) {
    static const QRegularExpression re(
        "(?:^|[^0-9])(20\\d\\d)[-_. ]?(0[1-9]|1[0-2])[-_. ]?(0[1-9]|[12]\\d|3[01])"
        "(?:[-_. T]?([01]\\d|2[0-3])[-_. ]?([0-5]\\d))?");
    auto m = re.match(name);
    if (!m.hasMatch()) return -1;
    int y = m.captured(1).toInt(), mo = m.captured(2).toInt(), d = m.captured(3).toInt();
    int h = m.captured(4).isEmpty() ? 0 : m.captured(4).toInt();
    int mi = m.captured(5).isEmpty() ? 0 : m.captured(5).toInt();
    QDateTime dt(QDate(y, mo, d), QTime(h, mi, 0), Qt::UTC);
    return dt.isValid() ? dt.toSecsSinceEpoch() : -1;
}

qint64 MediaStore::exifDate(const QString& path) {
    QJsonObject d = Probe::run(m_ffprobe, path);
    QMap<QString, QString> tags;
    auto merge = [&](const QJsonObject& t) {
        for (auto it = t.begin(); it != t.end(); ++it) tags[it.key()] = it.value().toString();
    };
    merge(d.value("format").toObject().value("tags").toObject());
    for (const auto& sv : d.value("streams").toArray())
        merge(sv.toObject().value("tags").toObject());

    static const QRegularExpression dtRe("(\\d{4})[-:](\\d\\d)[-:](\\d\\d)[ T](\\d\\d):(\\d\\d):(\\d\\d)");
    for (const QString& k : { "DateTimeOriginal", "DateTime", "creation_time", "date" }) {
        QString v = tags.value(k, tags.value(k.toLower()));
        if (v.isEmpty()) continue;
        auto m = dtRe.match(v);
        if (m.hasMatch()) {
            QDateTime dt(QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()),
                         QTime(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt()), Qt::UTC);
            if (dt.isValid()) return dt.toSecsSinceEpoch();
        }
    }
    return -1;
}

QJsonObject MediaStore::mediaInfo(const QString& path) {
    QFileInfo fi(path);
    QString name = fi.fileName();
    bool isImg = isImage(name);
    QJsonObject info;
    info["name"] = name;
    info["type"] = isImg ? "image" : "audio";
    info["size"] = fi.size();

    if (isImg) {
        auto [w, h] = Probe::probeImage(m_ffprobe, path);
        info["w"] = w; info["h"] = h;
        qint64 t = exifDate(path);
        info["dateSrc"] = "EXIF";
        if (t < 0) { t = nameDate(name); info["dateSrc"] = "naam"; }
        if (t < 0) { t = fi.lastModified().toSecsSinceEpoch(); info["dateSrc"] = "file"; }
        info["date"] = (double)(t * 1000);
    } else {
        info["duration"] = std::round(Probe::probeDuration(m_ffprobe, path) * 1000) / 1000.0;
        info["date"] = (double)(fi.lastModified().toSecsSinceEpoch() * 1000);
        info["dateSrc"] = "file";
        info["kind"] = getKind(name);
    }
    return info;
}

QJsonArray MediaStore::listMedia() {
    QJsonArray out;
    QDir dir(AppPaths::media());
    if (!dir.exists()) return out;
    QStringList names = dir.entryList(QDir::Files, QDir::Name);
    for (const auto& name : names) {
        if (name.startsWith('.')) continue;
        if (isImage(name) || isAudio(name))
            out.append(mediaInfo(dir.filePath(name)));
    }
    return out;
}

QString MediaStore::thumbFor(const QString& name) {
    QString src = QDir(AppPaths::media()).filePath(name);
    if (!QFile::exists(src)) return {};
    QFileInfo st(src);
    QString out = QDir(AppPaths::cache()).filePath(
        QString("th_%1_%2.jpg").arg(safeName(name)).arg(st.lastModified().toSecsSinceEpoch()));
    if (QFile::exists(out)) return out;

    QStringList args;
    if (isImage(name))
        args = { "-y", "-v", "error", "-i", src, "-vf", "scale=-2:200:force_original_aspect_ratio=decrease",
            "-frames:v", "1", "-q:v", "5", out };
    else
        args = { "-y", "-v", "error", "-i", src, "-filter_complex",
            "showwavespic=s=480x120:colors=#7cc4ff|#7cc4ff", "-frames:v", "1", out };

    QProcess p;
    p.start(m_ffmpeg, args);
    p.waitForFinished(60000);
    return QFile::exists(out) ? out : QString();
}

} // namespace rf
