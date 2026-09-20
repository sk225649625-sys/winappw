#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>

namespace rf {

/// Poori media library sirf local disk (AppPaths::media()) se aati hai.
/// Koi HTTP, koi server, koi IP kahin nahi -- editor.html ki jagah ab
/// native Qt widgets seedhe QImage/QPixmap se local file dikhate hain.
class MediaStore {
public:
    MediaStore(QString ffmpeg, QString ffprobe);

    static bool isImage(const QString& name);
    static bool isAudio(const QString& name);
    static QString safeName(const QString& name);

    QJsonObject mediaInfo(const QString& path);
    QJsonArray listMedia();
    QString thumbFor(const QString& name); // returns local cache file path (or empty)

    void setKind(const QString& name, const QString& kind);
    void dropKind(const QString& name);
    QString getKind(const QString& name, const QString& def = "music");

private:
    QMap<QString, QString> loadKinds();
    void saveKinds(const QMap<QString, QString>& d);
    qint64 exifDate(const QString& path);
    static qint64 nameDate(const QString& name);

    QString m_ffmpeg, m_ffprobe;
    QString m_kindsFile;
};

} // namespace rf
