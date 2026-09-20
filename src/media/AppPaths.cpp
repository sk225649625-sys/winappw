#include "AppPaths.h"
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <algorithm>

namespace rf {

QString AppPaths::findTool(const QString& name) {
    QString exe = name + ".exe";
    QStringList candidates = {
        QDir(ffmpegDir()).filePath("bin/" + exe),
        QDir(ffmpegDir()).filePath(exe),
        QDir(root()).filePath(exe),
    };
    for (const auto& c : candidates)
        if (QFile::exists(c)) return c;

    // system PATH me dhoondo
    auto pathVar = QProcessEnvironment::systemEnvironment().value("PATH");
#ifdef Q_OS_WIN
    QChar sep = ';';
#else
    QChar sep = ':';
#endif
    for (const auto& dir : pathVar.split(sep, Qt::SkipEmptyParts)) {
        QString full = QDir(dir).filePath(exe);
        if (QFile::exists(full)) return full;
    }
    return {};
}

QString AppPaths::findFont(const QStringList& candidateNames) {
    QStringList dirs = { "C:\\Windows\\Fonts", QDir(root()).filePath("fonts") };
    for (const auto& n : candidateNames) {
        for (const auto& d : dirs) {
            QString p = QDir(d).filePath(n);
            if (QFile::exists(p)) return p;
        }
    }
    // fallback: koi bhi bold .ttf (emoji/CJK chhod kar)
    for (const auto& d : dirs) {
        if (!QDir(d).exists()) continue;
        QDirIterator it(d, { "*.ttf" }, QDir::Files, QDirIterator::Subdirectories);
        QStringList found;
        QRegularExpression boldRe("(bold|semib|black)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpression skipRe("(emoji|japan|korea|cjk|arab|thai|symbol)", QRegularExpression::CaseInsensitiveOption);
        while (it.hasNext()) {
            QString p = it.next();
            QString name = QFileInfo(p).fileName();
            if (boldRe.match(name).hasMatch() && !skipRe.match(name).hasMatch())
                found << p;
        }
        if (!found.isEmpty()) {
            std::sort(found.begin(), found.end(), [](const QString& a, const QString& b) { return a.length() < b.length(); });
            return found.first();
        }
    }
    return {};
}

} // namespace rf
