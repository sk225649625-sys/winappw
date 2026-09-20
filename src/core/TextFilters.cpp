#include "TextFilters.h"
#include "Json.h"
#include "Num.h"
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

namespace rf {

static QString F(double v, int d = 3) { return QString::fromStdString(fmt(v, d)); }

QStringList TextFilters::wrapLines(const QString& text, int maxChars, int limit) {
    QStringList lines;
    QString cur;
    for (const QString& w : text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
        QString t = (cur + " " + w).trimmed();
        if (t.length() > maxChars && !cur.isEmpty()) {
            lines << cur;
            cur = w;
        } else cur = t;
    }
    if (!cur.isEmpty()) lines << cur;
    if (lines.size() > limit) lines = lines.mid(0, limit);
    return lines;
}

QString TextFilters::drawText(const QString& font, const QString& text, int fs, const QString& color,
                               const QString& x, const QString& y, const QString& alpha,
                               const QString& enable, bool border) {
    QStringList parts;
    parts << ("drawtext=fontfile='" + J::escQ(font) + "'");
    parts << ("text='" + J::escQ(text) + "'");
    parts << ("fontsize=" + QString::number(fs));
    parts << ("fontcolor=" + color);
    if (border) {
        parts << ("borderw=" + QString::number(std::max(2, (int)(fs * .08))));
        parts << "bordercolor=black@0.85";
        parts << "shadowcolor=black@0.45";
        parts << "shadowx=2";
        parts << "shadowy=3";
    }
    parts << ("x='" + x + "'");
    parts << ("y='" + y + "'");
    if (!alpha.isEmpty()) parts << ("alpha='" + alpha + "'");
    if (!enable.isEmpty()) parts << ("enable='" + enable + "'");
    return parts.join(":");
}

QStringList TextFilters::captionFilters(const QJsonObject& clip, int W, int H, const QString& font, double dur, double off) {
    QString txt = J::str(clip, "text", "").trimmed();
    if (txt.isEmpty() || font.isEmpty()) return {};

    int fs = std::max(12, (int)std::round(std::min(W, H) * .072));
    QStringList lines = wrapLines(txt, std::max(8, (int)((W * .84) / (fs * .52))));
    if (lines.isEmpty()) return {};

    double lh = fs * 1.25;
    QString pos = J::str(clip, "tpos", "bottom");
    double cy = pos == "top" ? H * .17 : pos == "center" ? H * .5 : H * .8;
    QString st = J::str(clip, "tstyle", "fadeup");
    QString T = "(t+" + F(off, 4) + ")";

    QString aIn;
    QString offY;
    bool hasOffY = false;
    if (st == "pop") { aIn = "min(1," + T + "*7)"; }
    else if (st == "slide") { aIn = "(1-pow(1-min(" + T + "/0.55,1),3))"; }
    else {
        aIn = "(1-pow(1-min(" + T + "/0.6,1),3))";
        offY = "(1-" + aIn + ")*" + F(H * .035, 3);
        hasOffY = true;
    }

    QString alpha = "max(0,min(1," + aIn + "))*min(1,max(0,(" + F(dur, 3) + "-" + T + ")/0.3))";
    QString xExpr = "(w-text_w)/2";
    if (st == "slide") xExpr = "(w-text_w)/2-(1-" + aIn + ")*" + F(W * .25, 1);

    QStringList outp;
    int n = lines.size();
    for (int i = 0; i < n; i++) {
        double y = cy + (i - (n - 1) / 2.0) * lh - fs * .5;
        QString yExpr = hasOffY ? (F(y, 1) + "+" + offY) : F(y, 1);
        QString enable;
        if (st == "type") {
            double start = 0;
            for (int k = 0; k < i; k++) start += lines[k].length() + 1;
            start /= 24.0;
            enable = "gte(" + T + "," + F(start, 3) + ")";
        }
        outp << drawText(font, lines[i], fs, "white", xExpr, yExpr, alpha, enable);
    }
    return outp;
}

QStringList TextFilters::overlayFilters(const QJsonArray& overlays, int W, int H, const QString& font,
                                         const QString& emojiFont, double t0) {
    QStringList outp;
    for (const auto& ov : overlays) {
        QJsonObject o = ov.toObject();
        QString txt = J::str(o, "text", "").trimmed();
        if (txt.isEmpty()) continue;
        int fs = std::max(10, (int)std::round(std::min(W, H) * J::num(o, "size", .09)));
        QString x = F(J::num(o, "x", .5) * W, 1) + "-text_w/2";
        QString y = F(J::num(o, "y", .5) * H, 1) + "-text_h/2";
        bool emoji = J::str(o, "type", "") == "emoji";
        QString f = emoji ? (emojiFont.isEmpty() ? font : emojiFont) : font;
        if (f.isEmpty()) continue;
        QString color = J::str(o, "color", "#ffffff").replace("#", "0x");
        QString enable;
        if (o.contains("t0") || o.contains("t1")) {
            double a = J::num(o, "t0", 0);
            double b = J::num(o, "t1", 1e6);
            enable = "between(t+" + F(t0, 4) + "," + F(a, 4) + "," + F(b, 4) + ")";
        }
        outp << drawText(f, txt, fs, emoji ? "white" : color, x, y, QString(), enable, !emoji);
    }
    return outp;
}

} // namespace rf
