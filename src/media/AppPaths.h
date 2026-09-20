#pragma once
#include <QString>
#include <QCoreApplication>
#include <QDir>

namespace rf {

// Sab kuch .exe ke bagal me (local disk) -- koi server, koi URL, koi IP
// kahin involved nahi.
class AppPaths {
public:
    static QString root() { return QCoreApplication::applicationDirPath(); }
    static QString media() { return QDir(root()).filePath("media"); }
    static QString projects() { return QDir(root()).filePath("projects"); }
    static QString exports() { return QDir(root()).filePath("exports"); }
    static QString cache() { return QDir(root()).filePath("cache"); }

    // Apna ffmpeg folder -- agar system PATH me nahi hai to yahan set karo,
    // ya .exe ke bagal me ffmpeg.exe/ffprobe.exe rakh do (auto-detect).
    static QString ffmpegDir() { return "C:\\ffmpeg-8.1.1-essentials_build"; }

    static void ensureFolders() {
        for (const QString& d : { media(), projects(), exports(), cache() })
            QDir().mkpath(d);
    }

    static QString findTool(const QString& name);
    static QString findFont(const QStringList& candidateNames);
};

} // namespace rf
