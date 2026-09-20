#include "RenderEngine.h"
#include "Json.h"
#include "Num.h"
#include "Catalogs.h"
#include "MotionExpr.h"
#include "TextFilters.h"
#include "Atmos.h"
#include "Probe.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <thread>
#include <cmath>
#include <algorithm>

namespace rf {

static QString F(double v, int d = 4) { return QString::fromStdString(fmt(v, d)); }

RenderEngine::RenderEngine(QString ffmpeg, QString ffprobe, QJsonObject project, QString outPath, QString mediaDir,
                            bool fast, QString font, QString emojiFont, int workers)
    : m_ffmpeg(std::move(ffmpeg)), m_ffprobe(std::move(ffprobe)), m_out(std::move(outPath)),
      m_mediaDir(std::move(mediaDir)), m_font(std::move(font)), m_emojiFont(std::move(emojiFont)),
      m_project(std::move(project)), m_fast(fast),
      m_workers(workers > 0 ? workers : Catalogs::cpuCount()) {}

std::pair<int, int> RenderEngine::size() const {
    double rw = 16, rh = 9;
    QStringList parts = J::str(m_project, "ratio", "16:9").split(':');
    if (parts.size() == 2) {
        bool ok1 = false, ok2 = false;
        double a = parts[0].toDouble(&ok1), b = parts[1].toDouble(&ok2);
        if (ok1 && ok2 && a > 0 && b > 0) { rw = a; rh = b; }
    }
    int shortSide = (int)J::num(m_project, "quality", 1080);
    if (m_fast) shortSide = std::min(shortSide, 480);
    shortSide = (int)std::round(std::max(180, std::min(2160, shortSide)) / 2.0) * 2;
    int longSide = (int)std::round(shortSide * std::max(rw, rh) / std::min(rw, rh) / 2.0) * 2;
    return rw >= rh ? std::make_pair(longSide, shortSide) : std::make_pair(shortSide, longSide);
}

int RenderEngine::fps() const {
    int f = (int)J::num(m_project, "fps", 30);
    if (m_fast) f = std::min(f, 24);
    return std::max(1, std::min(60, f));
}

QString RenderEngine::mediaPath(const QString& name) const {
    QFileInfo fi(name);
    if (fi.isAbsolute()) return name;
    return QDir(m_mediaDir).filePath(fi.fileName());
}

TimelineData RenderEngine::timeline() const {
    TimelineData td;
    QJsonArray all = J::arr(m_project, "clips");
    for (const auto& cv : all) {
        QJsonObject c = cv.toObject();
        if (!J::str(c, "file").isEmpty()) td.clips.append(c);
    }
    if (td.clips.isEmpty())
        throw std::runtime_error("Timeline khaali hai -- pehle photos add karo.");

    for (const auto& cv : td.clips) td.durs << std::max(0.2, J::num(cv.toObject(), "dur", 3));

    double t = 0.0;
    for (double d : td.durs) { td.starts << t; t += d; }
    td.total = t;

    for (int i = 0; i < td.clips.size(); i++) {
        QJsonObject c = td.clips[i].toObject();
        if (i == 0) { td.tds << 0.0; continue; }
        QString transKey = J::str(c, "trans", "fade");
        // .value() bina default ke -> key na mile ya "none" ho, dono me khaali
        // QString milti hai (Python me TRANS_MAP.get(key) dono jagah None deta hai).
        QString kind = Catalogs::transMap().value(transKey);
        if (kind.isEmpty())
            td.tds << 0.04; // hard cut
        else
            td.tds << std::max(0.05, std::min({ J::num(c, "transDur", .6), td.durs[i] * .6, td.durs[i - 1] * .6 }));
    }
    return td;
}

QString RenderEngine::clipChain(QStringList& graph, QStringList& inputs, const QJsonObject& c, int idx,
                                 int W, int H, int BW, int BH, double S, int fpsV, double length, int clipFrames,
                                 double off, double durForCaption, const QString& label) const {
    QString path = mediaPath(J::str(c, "file"));
    if (!QFile::exists(path)) throw std::runtime_error(("File nahi mili: " + QFileInfo(path).fileName()).toStdString());
    auto [iw, ih] = Probe::probeImage(m_ffprobe, path);
    int frames = std::max(2, (int)std::round(length * fpsV));

    inputs << "-framerate" << QString::number(fpsV) << "-i" << path;
    QString src = QString::number(idx) + ":v";

    QString fit = J::str(c, "fit", "auto");
    if (fit != "auto" && fit != "cover" && fit != "contain") fit = "auto";
    if (fit == "auto")
        fit = std::fabs(std::log((iw / (double)ih) / (W / (double)H))) < .28 ? "cover" : "contain";
    QString motionName = J::str(c, "motion", "kenIn");
    if (motionName == "float" || motionName == "slideIn") fit = "contain";

    QString baseLbl = "b" + label;
    if (fit == "cover") {
        graph << QString("[%1]scale=%2:%3:force_original_aspect_ratio=increase:flags=bicubic,crop=%2:%3,setsar=1[%4]")
            .arg(src).arg(BW).arg(BH).arg(baseLbl);
    } else {
        int sbw = std::max(16, BW / Catalogs::kBlurDiv), sbh = std::max(16, BH / Catalogs::kBlurDiv);
        graph << QString("[%1]split=2[sa%2][sb%2]").arg(src, label);
        graph << QString("[sa%1]scale=%2:%3:force_original_aspect_ratio=increase,crop=%2:%3,boxblur=6:2,scale=%4:%5,eq=brightness=-0.12,setsar=1[bg%1]")
            .arg(label).arg(sbw).arg(sbh).arg(BW).arg(BH);
        graph << QString("[sb%1]scale=%2:%3:force_original_aspect_ratio=decrease:flags=bicubic,setsar=1[fg%1]").arg(label).arg(BW).arg(BH);
        graph << QString("[bg%1][fg%1]overlay=(W-w)/2:(H-h)/2[%2]").arg(label, baseLbl);
    }

    graph << QString("[%1]loop=loop=%2:size=1:start=0,trim=start_frame=0:end_frame=%3,setpts=N/%4/TB[l%5]")
        .arg(baseLbl).arg(frames + 2).arg(frames).arg(fpsV).arg(label);
    QString cur = "l" + label;

    MotionResult mo = MotionExpr::compute(c, W, H, fpsV, clipFrames, off);
    QString z = mo.z, tx = mo.tx, ty = mo.ty;
    auto bm = MotionExpr::beatMult(J::obj(m_project.value("fxg")), fpsV, off);
    if (bm) z = "(" + z + ")*" + *bm;
    if (motionName != "float") z = "max(1.0," + z + ")";
    z = "min(" + F(S * 1.5) + "," + z + ")";

    if (mo.rot) {
        graph << QString("[%1]rotate=a='%2':ow=iw:oh=ih:c=black[r%3]").arg(cur, *mo.rot, label);
        cur = "r" + label;
    }

    QString xExpr = "iw/2-(iw/zoom/2)-(" + tx + ")*" + F(S, 5);
    QString yExpr = "ih/2-(ih/zoom/2)-(" + ty + ")*" + F(S, 5);
    graph << QString("[%1]zoompan=z='%2':x='%3':y='%4':d=1:s=%5x%6:fps=%7[z%8]")
        .arg(cur, z, xExpr, yExpr).arg(W).arg(H).arg(fpsV).arg(label);
    cur = "z" + label;

    QStringList chain;
    QString look = Catalogs::looks().value(J::str(c, "look", "none"), "");
    if (!look.isEmpty()) chain << look;
    chain += TextFilters::captionFilters(c, W, H, m_font, durForCaption, off);
    chain << "format=yuv420p" << "setpts=PTS-STARTPTS";
    QString outLbl = "v" + label;
    graph << QString("[%1]%2[%3]").arg(cur, chain.join(","), outLbl);
    return outLbl;
}

RenderEngine::ChunkResult RenderEngine::buildChunk(int i0, int i1, const QString& outPath) const {
    TimelineData td = timeline();
    auto [W, H] = size();
    int fpsV = fps();
    int BW = (int)std::round(W * Catalogs::kOversample / 2) * 2;
    int BH = (int)std::round(H * Catalogs::kOversample / 2) * 2;
    double S = BW / (double)W;
    QJsonObject fxg = J::obj(m_project.value("fxg"));

    double leadTd = i0 > 0 ? td.tds[i0] : 0.0;
    double chunkStart = td.starts[i0];
    double chunkEnd = i1 < td.clips.size() ? td.starts[i1] : td.total;

    QStringList inputs, graph;
    int idx = 0;
    QList<std::pair<QString, double>> segs;

    if (leadTd > 0) {
        QJsonObject prev = td.clips[i0 - 1].toObject();
        int pf = std::max(2, (int)std::round(td.durs[i0 - 1] * fpsV));
        QString lbl = clipChain(graph, inputs, prev, idx, W, H, BW, BH, S, fpsV, leadTd, pf, td.durs[i0 - 1], td.durs[i0 - 1], "L");
        idx++;
        segs << std::make_pair(lbl, td.starts[i0]);
    }

    for (int k = i0; k < i1; k++) {
        QJsonObject c = td.clips[k].toObject();
        double extra = (k + 1 < td.clips.size()) ? td.tds[k + 1] : 0.0;
        double length = td.durs[k] + extra;
        int cf = std::max(2, (int)std::round(td.durs[k] * fpsV));
        QString lbl = clipChain(graph, inputs, c, idx, W, H, BW, BH, S, fpsV, length, cf, 0.0, td.durs[k], QString::number(k));
        idx++;
        segs << std::make_pair(lbl, td.starts[k]);
    }

    QString acc = segs[0].first;
    for (int n = 1; n < segs.size(); n++) {
        int k = i0 + n - (leadTd > 0 ? 1 : 0);
        QString transKey = J::str(td.clips[k].toObject(), "trans", "fade");
        QString kind = Catalogs::transMap().value(transKey, "fade");
        if (kind.isEmpty()) kind = "fade";
        double tdv = td.tds[k];
        double rel = segs[n].second - chunkStart;
        QString outLbl = "x" + QString::number(n);
        graph << QString("[%1][%2]xfade=transition=%3:duration=%4:offset=%5[%6]")
            .arg(acc, segs[n].first, kind, F(std::max(0.02, tdv)), F(std::max(0.0, rel)), outLbl);
        acc = outLbl;
    }

    QStringList post;
    if (J::boolean(fxg, "grain") && !m_fast) post << "noise=alls=9:allf=t+u";
    if (J::boolean(fxg, "vignette")) post << "vignette=PI/4.2";
    post << ("fps=" + QString::number(fpsV)) << "format=yuv420p";
    graph << QString("[%1]%2[pre]").arg(acc, post.join(","));
    QString cur = "pre";

    if (!m_fast) {
        auto [lines, blends] = Atmos::layers(fxg, W, H, fpsV, chunkStart);
        graph += lines;
        for (int n = 0; n < blends.size(); n++) {
            QString nxt = "at" + QString::number(n);
            graph << QString("[%1][%2]blend=all_mode=screen:shortest=1[%3]").arg(cur, blends[n], nxt);
            cur = nxt;
        }
    }

    QStringList post2;
    if (J::boolean(fxg, "letterbox")) {
        int bar = std::max(0, (int)((H - W / 2.39) / 2));
        if (bar > 0) {
            post2 << QString("drawbox=x=0:y=0:w=%1:h=%2:color=black:t=fill").arg(W).arg(bar);
            post2 << QString("drawbox=x=0:y=%1:w=%2:h=%3:color=black:t=fill").arg(H - bar).arg(W).arg(bar);
        }
    }
    post2 += TextFilters::overlayFilters(J::arr(m_project, "overlays"), W, H, m_font, m_emojiFont, chunkStart);
    bool fadeOn = fxg.contains("fade") ? J::boolean(fxg, "fade", true) : true;
    if (fadeOn && td.total > 1.2) {
        if (chunkStart < 0.5)
            post2 << ("fade=t=in:st=" + F(std::max(0.0, -chunkStart), 3) + ":d=0.5");
        double foSt = td.total - 0.6 - chunkStart;
        if (foSt < (chunkEnd - chunkStart))
            post2 << ("fade=t=out:st=" + F(std::max(0.0, foSt), 3) + ":d=0.6");
    }
    post2 << "format=yuv420p";
    graph << QString("[%1]%2[vout]").arg(cur, post2.join(","));

    double dur = std::max(0.05, chunkEnd - chunkStart);
    QString preset = m_fast ? "ultrafast" : J::str(m_project, "preset", "veryfast");
    QString crf = m_fast ? "28" : QString::number((int)J::num(m_project, "crf", 20));

    QStringList cmd = { m_ffmpeg, "-y", "-hide_banner", "-loglevel", "error", "-stats",
        "-threads", (m_workers > 1 ? "1" : "0") };
    cmd += inputs;
    cmd += { "-filter_complex_script", "%GRAPH%", "-map", "[vout]",
        "-an", "-c:v", "libx264", "-preset", preset, "-crf", crf,
        "-pix_fmt", "yuv420p", "-r", QString::number(fpsV), "-g", QString::number(fpsV * 2),
        "-t", F(dur), outPath };

    return { cmd, graph.join(";\n"), dur, chunkStart };
}

QString RenderEngine::still(double t, const QString& outPath, int maxW) const {
    TimelineData td = timeline();
    t = std::max(0.0, std::min(t, std::max(0.0, td.total - 0.001)));
    int k = 0;
    for (int i = 0; i < td.starts.size(); i++)
        if (td.starts[i] <= t + 1e-6) k = i;
    double off = std::max(0.0, std::min(t - td.starts[k], td.durs[k]));

    auto [W0, H0] = size();
    int W = W0, H = H0;
    if (W > maxW) {
        H = (int)std::round(H * maxW / (double)W / 2.0) * 2;
        W = (int)std::round(maxW / 2.0) * 2;
    }
    int fpsV = fps();
    int BW = (int)std::round(W * Catalogs::kOversample / 2) * 2;
    int BH = (int)std::round(H * Catalogs::kOversample / 2) * 2;
    double S = BW / (double)W;
    QJsonObject fxg = J::obj(m_project.value("fxg"));

    QStringList inputs, graph;
    int cf = std::max(2, (int)std::round(td.durs[k] * fpsV));
    QJsonObject clipK = td.clips[k].toObject();
    QString lbl = clipChain(graph, inputs, clipK, 0, W, H, BW, BH, S, fpsV, 2.0 / fpsV, cf, off, td.durs[k], "P");

    QStringList post;
    if (J::boolean(fxg, "vignette")) post << "vignette=PI/4.2";
    post << "format=yuv420p";
    graph << QString("[%1]%2[pre]").arg(lbl, post.join(","));
    QString cur = "pre";
    auto [lines, blends] = Atmos::layers(fxg, W, H, fpsV, t);
    graph += lines;
    for (int n = 0; n < blends.size(); n++) {
        graph << QString("[%1][%2]blend=all_mode=screen:shortest=1[at%3]").arg(cur, blends[n]).arg(n);
        cur = "at" + QString::number(n);
    }
    QStringList post2;
    if (J::boolean(fxg, "letterbox")) {
        int bar = std::max(0, (int)((H - W / 2.39) / 2));
        if (bar > 0) {
            post2 << QString("drawbox=x=0:y=0:w=%1:h=%2:color=black:t=fill").arg(W).arg(bar);
            post2 << QString("drawbox=x=0:y=%1:w=%2:h=%3:color=black:t=fill").arg(H - bar).arg(W).arg(bar);
        }
    }
    post2 += TextFilters::overlayFilters(J::arr(m_project, "overlays"), W, H, m_font, m_emojiFont, t);
    post2 << "format=yuvj420p";
    graph << QString("[%1]%2[vout]").arg(cur, post2.join(","));

    QTemporaryDir tmp;
    QString gf = tmp.filePath("g.txt");
    QFile f(gf);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    f.write(graph.join(";\n").toUtf8());
    f.close();

    QStringList args = { "-y", "-hide_banner", "-loglevel", "error" };
    args += inputs;
    args += { "-filter_complex_script", gf, "-map", "[vout]", "-frames:v", "1", "-q:v", "4", outPath };

    QProcess p;
    p.start(m_ffmpeg, args);
    p.waitForFinished(60000);
    if (p.exitCode() != 0 || !QFile::exists(outPath)) {
        QByteArray err = p.readAllStandardError();
        throw std::runtime_error(err.right(400).toStdString());
    }
    return outPath;
}

RenderEngine::AudioResult RenderEngine::buildAudio(const QString& outPath, double total) const {
    AudioResult res;
    QJsonArray aclipsAll = J::arr(m_project, "audio");
    QList<QJsonObject> aclips;
    for (const auto& a : aclipsAll) { QJsonObject o = a.toObject(); if (!J::str(o, "file").isEmpty()) aclips << o; }

    QList<QJsonObject> vclips;
    QJsonValue vnode = m_project.value("voice");
    if (vnode.isObject()) {
        QJsonObject vo = vnode.toObject();
        if (!J::str(vo, "file").isEmpty()) vclips << vo;
    } else if (vnode.isArray()) {
        for (const auto& v : vnode.toArray()) {
            QJsonObject vo = v.toObject();
            if (!J::str(vo, "file").isEmpty()) vclips << vo;
        }
    }

    QStringList inputs, graph, labels;

    for (const auto& a : aclips) {
        QString path = mediaPath(J::str(a, "file"));
        if (!QFile::exists(path)) continue;
        double start = std::max(0.0, J::num(a, "start", 0));
        if (start >= total) continue;
        double dur = std::min(std::max(0.05, J::num(a, "dur", 1)), total - start);
        double off = std::max(0.0, J::num(a, "offset", 0));
        if (J::boolean(a, "loop")) inputs << "-stream_loop" << "-1";
        inputs << "-ss" << F(off) << "-t" << F(dur) << "-i" << path;

        QStringList ch = { "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo",
            "asetpts=PTS-STARTPTS", "volume=" + F(std::max(0.0, J::num(a, "vol", 1))) };
        double fi = J::num(a, "fadeIn", 0), fo = J::num(a, "fadeOut", 0);
        if (fi > .01) ch << ("afade=t=in:st=0:d=" + F(fi, 3));
        if (fo > .01) ch << ("afade=t=out:st=" + F(std::max(0.0, dur - fo), 3) + ":d=" + F(fo, 3));
        if (start > .001) { int ms = (int)(start * 1000); ch << QString("adelay=%1|%1").arg(ms); }
        QString lbl = "a" + QString::number(labels.size());
        graph << QString("[%1:a]%2[%3]").arg(labels.size()).arg(ch.join(",")).arg(lbl);
        labels << lbl;
    }

    int nMusicInputs = labels.size();
    QString mixLbl;
    if (!labels.isEmpty()) {
        if (labels.size() == 1) graph << QString("[%1]anull[mmix]").arg(labels[0]);
        else {
            QString ins; for (auto& l : labels) ins += "[" + l + "]";
            graph << QString("%1amix=inputs=%2:normalize=0:dropout_transition=0[mmix]").arg(ins).arg(labels.size());
        }
        QStringList fx = Atmos::audioFxChain(J::obj(m_project.value("afx")));
        QStringList tail = fx.isEmpty() ? QStringList{ "anull" } : fx;
        tail += { "apad", "atrim=0:" + F(total), "asetpts=PTS-STARTPTS",
            "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo" };
        graph << QString("[mmix]%1[mfx]").arg(tail.join(","));
        mixLbl = "mfx";
    }

    QStringList voiceLbls;
    double cursor = 0.0;
    int vidx = nMusicInputs;
    for (const auto& v : vclips) {
        QString vpath = mediaPath(J::str(v, "file"));
        if (!QFile::exists(vpath)) continue;
        double vstart = std::max(0.0, J::num(v, "start", cursor));
        double vdurWant = std::max(0.05, J::num(v, "dur", 3));
        cursor = vstart + vdurWant;
        if (vstart >= total) continue;
        double vdur = std::min(vdurWant, total - vstart);
        if (vdur <= 0.02) continue;
        double voff = std::max(0.0, J::num(v, "offset", 0));
        inputs << "-ss" << F(voff) << "-t" << F(vdur) << "-i" << vpath;

        QStringList ch = { "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo",
            "asetpts=PTS-STARTPTS", "volume=" + F(std::max(0.0, J::num(v, "vol", 1))) };
        double fi = J::num(v, "fadeIn", 0), fo = J::num(v, "fadeOut", 0);
        if (fi > .01) ch << ("afade=t=in:st=0:d=" + F(std::min(fi, vdur), 3));
        if (fo > .01) ch << ("afade=t=out:st=" + F(std::max(0.0, vdur - fo), 3) + ":d=" + F(std::min(fo, vdur), 3));
        if (vstart > .001) { int ms = (int)std::round(vstart * 1000); ch << QString("adelay=%1|%1").arg(ms); }
        ch += { "apad", "atrim=0:" + F(total), "asetpts=PTS-STARTPTS",
            "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo" };
        QString lbl = "v" + QString::number(voiceLbls.size());
        graph << QString("[%1:a]%2[%3]").arg(vidx).arg(ch.join(",")).arg(lbl);
        voiceLbls << lbl;
        vidx++;
    }

    QString voiceLbl;
    if (voiceLbls.size() == 1) { graph << QString("[%1]anull[voc]").arg(voiceLbls[0]); voiceLbl = "voc"; }
    else if (voiceLbls.size() > 1) {
        QString ins; for (auto& l : voiceLbls) ins += "[" + l + "]";
        graph << QString("%1amix=inputs=%2:normalize=0:dropout_transition=0,"
                          "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo[voc]").arg(ins).arg(voiceLbls.size());
        voiceLbl = "voc";
    }

    if (mixLbl.isEmpty() && voiceLbl.isEmpty()) { res.has = false; return res; }
    if (!mixLbl.isEmpty() && !voiceLbl.isEmpty())
        graph << QString("[%1][%2]amix=inputs=2:normalize=0:dropout_transition=0,alimiter=limit=0.97[aout]").arg(mixLbl, voiceLbl);
    else
        graph << QString("[%1]alimiter=limit=0.97[aout]").arg(mixLbl.isEmpty() ? voiceLbl : mixLbl);

    QStringList cmd = { m_ffmpeg, "-y", "-hide_banner", "-loglevel", "error" };
    cmd += inputs;
    cmd += { "-filter_complex_script", "%GRAPH%", "-map", "[aout]", "-c:a", "aac", "-b:a", "192k",
        "-t", F(total), outPath };

    res.has = true;
    res.cmd = cmd;
    res.graph = graph.join(";\n");
    return res;
}

void RenderEngine::cancel() {
    m_cancel = true;
    std::lock_guard<std::mutex> lk(m_lock);
    for (auto* p : m_procs)
        if (p->state() != QProcess::NotRunning) p->kill();
}

void RenderEngine::spawn(QStringList cmd, const QString& graph, const QString& tmpDir, const QString& tag,
                          int framesExpected, const std::function<void(double)>& progress) {
    QString gf = QDir(tmpDir).filePath("graph_" + tag + ".txt");
    QFile gfFile(gf);
    gfFile.open(QIODevice::WriteOnly | QIODevice::Text);
    gfFile.write(graph.toUtf8());
    gfFile.close();

    for (auto& a : cmd) if (a == "%GRAPH%") a = gf;
    QString exe = cmd.takeFirst();

    auto* proc = new QProcess();
    proc->setProgram(exe);
    proc->setArguments(cmd);
    proc->start();
    if (!proc->waitForStarted(10000)) {
        delete proc;
        throw std::runtime_error("ffmpeg start nahi hua");
    }
    { std::lock_guard<std::mutex> lk(m_lock); m_procs.push_back(proc); }

    static const QRegularExpression frameRe("frame=\\s*(\\d+)");
    QStringList tail;
    QByteArray leftover;
    while (proc->state() != QProcess::NotRunning) {
        proc->waitForReadyRead(100);
        QByteArray chunk = proc->readAllStandardError();
        if (!chunk.isEmpty()) {
            leftover += chunk;
            int nl;
            while ((nl = leftover.indexOf('\n')) >= 0) {
                QString line = QString::fromUtf8(leftover.left(nl)).trimmed();
                leftover.remove(0, nl + 1);
                if (line.isEmpty()) continue;
                tail << line;
                if (tail.size() > 30) tail.removeFirst();
                auto m = frameRe.match(line);
                if (m.hasMatch() && framesExpected > 0) {
                    std::lock_guard<std::mutex> lk(m_lock);
                    m_done[tag] = std::min(1.0, m.captured(1).toInt() / (double)framesExpected);
                    if (progress) progress(overall());
                }
            }
        }
    }
    proc->waitForFinished(5000);
    int exitCode = proc->exitCode();
    delete proc;

    { std::lock_guard<std::mutex> lk(m_lock); m_done[tag] = 1.0; }
    if (progress) progress(overall());
    if (exitCode != 0)
        throw std::runtime_error(("FFmpeg fail:\n" + tail.join("\n")).toStdString());
}

double RenderEngine::overall() {
    if (m_totalWeight <= 0) return 0.0;
    double s = 0;
    for (auto& [tag, w] : m_weights) s += (m_done.count(tag) ? m_done[tag] : 0.0) * w;
    return std::max(0.0, std::min(1.0, s / m_totalWeight));
}

RenderResult RenderEngine::run(std::function<void(double)> progress) {
    QElapsedTimer sw; sw.start();
    TimelineData td = timeline();
    m_duration = td.total;
    int fpsV = fps();
    int n = td.clips.size();

    int workers = std::max(1, std::min(m_workers, (int)std::ceil(n / (double)Catalogs::kMinClipsPerChunk)));
    int per = std::max(Catalogs::kMinClipsPerChunk, (int)std::ceil(n / (double)workers));
    QList<std::pair<int, int>> bounds;
    int i = 0;
    while (i < n) {
        int j = std::min(n, i + per);
        if (n - j < Catalogs::kMinClipsPerChunk) j = n;
        bounds << std::make_pair(i, j);
        i = j;
    }

    QTemporaryDir tmp;
    tmp.setAutoRemove(true);
    QString tmpPath = tmp.path();

    struct Job { QStringList cmd; QString graph; QString tag; int frames; };
    QList<Job> jobs;
    QStringList parts;
    m_weights.clear(); m_done.clear();

    for (int k = 0; k < bounds.size(); k++) {
        auto [a, b] = bounds[k];
        QString part = QDir(tmpPath).filePath(QString("part%1.mp4").arg(k, 2, 10, QChar('0')));
        auto cr = buildChunk(a, b, part);
        int frames = std::max(1, (int)(cr.dur * fpsV));
        QString tag = "v" + QString::number(k);
        m_weights[tag] = frames;
        jobs << Job{ cr.cmd, cr.graph, tag, frames };
        parts << part;
    }

    QString apath = QDir(tmpPath).filePath("audio.m4a");
    auto audioRes = buildAudio(apath, td.total);
    if (audioRes.has) {
        m_weights["a"] = std::max(1.0, td.total * fpsV * 0.06);
        jobs << Job{ audioRes.cmd, audioRes.graph, "a", 0 };
    }
    m_totalWeight = 0; for (auto& [k2, w] : m_weights) m_totalWeight += w;

    std::exception_ptr firstError;
    std::mutex errLock;
    std::vector<std::thread> threads;
    for (auto& job : jobs) {
        threads.emplace_back([&, job]() {
            try { spawn(job.cmd, job.graph, tmpPath, job.tag, job.frames, progress); }
            catch (...) {
                std::lock_guard<std::mutex> lk(errLock);
                if (!firstError) firstError = std::current_exception();
            }
        });
    }
    for (auto& th : threads) th.join();

    if (m_cancel) throw CancelledException();
    if (firstError) std::rethrow_exception(firstError);

    QString listf = QDir(tmpPath).filePath("parts.txt");
    QFile lf(listf);
    lf.open(QIODevice::WriteOnly | QIODevice::Text);
    for (auto& p : parts) {
        QString esc = QString(p).replace("\\", "/").replace("'", "'\\''");
        lf.write(("file '" + esc + "'\n").toUtf8());
    }
    lf.close();

    QStringList cmd2 = { "-y", "-hide_banner", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", listf };
    if (audioRes.has) cmd2 += { "-i", apath, "-map", "0:v:0", "-map", "1:a:0", "-shortest" };
    cmd2 += { "-c", "copy", "-movflags", "+faststart", m_out };

    QProcess proc;
    proc.start(m_ffmpeg, cmd2);
    proc.waitForFinished(-1);
    if (proc.exitCode() != 0 || !QFile::exists(m_out)) {
        QByteArray err = proc.readAllStandardError();
        throw std::runtime_error(("Jodne me dikkat: " + err.right(500)).toStdString());
    }
    if (progress) progress(1.0);

    RenderResult r;
    r.file = m_out;
    r.duration = td.total;
    r.seconds = sw.elapsed() / 1000.0;
    r.size = QFileInfo(m_out).size();
    r.chunks = parts.size();
    return r;
}

} // namespace rf
