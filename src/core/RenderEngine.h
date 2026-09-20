#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <map>

namespace rf {

class CancelledException : public std::exception {
public:
    const char* what() const noexcept override { return "cancelled"; }
};

struct RenderResult {
    QString file;
    double duration = 0;
    double seconds = 0;
    qint64 size = 0;
    int chunks = 0;
};

struct TimelineData {
    QJsonArray clips;
    QList<double> durs, starts, tds;
    double total = 0;
};

/// Port of reelforge_engine.py's Render class. Poora ffmpeg orchestration
/// (filter-graph banana, parallel chunks render karna, jodna) yahin hota
/// hai. ffmpeg/ffprobe SIRF local subprocess ki tarah call hote hain
/// (QProcess se, koi network involved nahi).
class RenderEngine {
public:
    RenderEngine(QString ffmpeg, QString ffprobe, QJsonObject project, QString outPath, QString mediaDir,
                 bool fast = false, QString font = {}, QString emojiFont = {}, int workers = -1);

    std::pair<int, int> size() const;
    int fps() const;
    TimelineData timeline() const;

    struct ChunkResult { QStringList cmd; QString graph; double dur; double chunkStart; };
    ChunkResult buildChunk(int i0, int i1, const QString& outPath) const;

    QString still(double t, const QString& outPath, int maxW = 720) const;

    struct AudioResult { bool has = false; QStringList cmd; QString graph; };
    AudioResult buildAudio(const QString& outPath, double total) const;

    void cancel();
    RenderResult run(std::function<void(double)> progress = nullptr);

    double duration() const { return m_duration; }

private:
    QString clipChain(QStringList& graph, QStringList& inputs, const QJsonObject& c, int idx,
                       int W, int H, int BW, int BH, double S, int fps, double length, int clipFrames,
                       double off, double durForCaption, const QString& label) const;

    QString mediaPath(const QString& name) const;
    void spawn(QStringList cmd, const QString& graph, const QString& tmpDir, const QString& tag,
               int framesExpected, const std::function<void(double)>& progress);
    double overall();

    QString m_ffmpeg, m_ffprobe, m_out, m_mediaDir, m_font, m_emojiFont;
    QJsonObject m_project;
    bool m_fast;
    int m_workers;
    double m_duration = 0;

    std::atomic<bool> m_cancel{false};
    std::mutex m_lock;
    std::vector<QProcess*> m_procs;
    std::map<QString, double> m_weights, m_done;
    double m_totalWeight = 0;
};

} // namespace rf
