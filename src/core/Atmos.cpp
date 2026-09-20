#include "Atmos.h"
#include "Json.h"
#include "Num.h"
#include <cmath>
#include <algorithm>

namespace rf {

static QString F(double v, int d = 4) { return QString::fromStdString(fmt(v, d)); }
static QString F0(double v) { return QString::number((int)std::llround(v)); }

std::pair<QStringList, QStringList> Atmos::layers(const QJsonObject& fxg, int W, int H, int fps, double t0) {
    QStringList lines, blends;
    const int GW = 128, GH = 72;
    QString shift = "setpts=PTS+" + F(t0, 5) + "/TB";
    QString unshift = "setpts=PTS-" + F(t0, 5) + "/TB";

    auto add = [&](const QString& name, const QString& chain) {
        lines << QString("color=c=black:s=%1x%2:r=%3,%4,%5,%6[%7]")
            .arg(GW).arg(GH).arg(fps).arg(shift).arg(chain).arg(unshift).arg(name);
        blends << name;
    };

    auto amt = [&](const QString& k, double d) {
        return std::max(0.0, std::min(1.5, J::num(fxg, k, d)));
    };

    if (J::boolean(fxg, "leaks")) {
        double a = amt("leakAmt", .6);
        QString blob = QString("%1*exp(-(pow(X-(%2+%3*sin(T*0.5)),2)+pow(Y-(%4+%5*cos(T*0.4)),2))/%6)")
            .arg(F0(150 * a)).arg(F0(GW * .5)).arg(F0(GW * .35)).arg(F0(GH * .45)).arg(F0(GH * .3)).arg(F0(GW * 4.5));
        add("leakL", "format=gray,geq=lum='" + blob + "',format=yuv420p,"
            "colorchannelmixer=rr=1:gg=0.52:bb=0.18,scale=" + QString::number(W) + ":" + QString::number(H) + ",boxblur=10:1");
    }

    if (J::boolean(fxg, "lights")) {
        double a = amt("lightAmt", .6);
        struct Spec { double fx, fy, sp; int r; };
        Spec specs[3] = { {.22, .3, .55, 55}, {.7, .62, .42, 40}, {.48, .8, .63, 30} };
        QStringList blobParts;
        for (int k = 0; k < 3; k++) {
            const auto& s = specs[k];
            blobParts << QString("%1*exp(-(pow(X-(%2+%3*sin(T*%4+%5)),2)+pow(Y-(%6+%7*cos(T*%8+%9)),2))/%10)")
                .arg(F0(200 * a)).arg(F0(GW * s.fx)).arg(F0(GW * .18)).arg(F(s.sp, 2)).arg(k)
                .arg(F0(GH * s.fy)).arg(F0(GH * .16)).arg(F(s.sp * .8, 2)).arg(k).arg(s.r);
        }
        add("lightL", "format=gray,geq=lum='" + blobParts.join("+") + "',format=yuv420p,"
            "colorchannelmixer=rr=0.9:gg=0.85:bb=1,scale=" + QString::number(W) + ":" + QString::number(H) + ",boxblur=6:1");
    }

    if (J::boolean(fxg, "dust")) {
        double a = amt("dustAmt", .5);
        int th = (int)(252 - 8 * a);
        lines << QString("color=c=black:s=%1x%2:r=%3,noise=alls=100:allf=t+u,"
                          "lut=y='if(gt(val\\,%4)\\,val\\,0)':u=128:v=128[dustL]").arg(W).arg(H).arg(fps).arg(th);
        blends << "dustL";
    }

    if (J::boolean(fxg, "sparkle")) {
        double a = amt("sparkAmt", .5);
        int th = (int)(253 - 6 * a);
        lines << QString("color=c=black:s=%1x%2:r=%3,noise=alls=100:allf=t+u,"
                          "lut=y='if(gt(val\\,%4)\\,255\\,0)':u=128:v=128,boxblur=2:1[sparkL]").arg(W).arg(H).arg(fps).arg(th);
        blends << "sparkL";
    }

    return { lines, blends };
}

QStringList Atmos::audioFxChain(const QJsonObject& afx) {
    QStringList ch;
    auto g = [&](const QString& k) -> std::pair<bool, double> {
        QJsonObject v = J::obj(afx.value(k));
        return { J::boolean(v, "on"), std::max(0.0, std::min(1.0, J::num(v, "amt", .5))) };
    };

    QJsonObject pv = J::obj(afx.value("pitch"));
    if (J::boolean(pv, "on") && std::fabs(J::num(pv, "amt", 0)) > .01) {
        double semis = J::num(pv, "amt", 0) * 6 / 12.0;
        double rate = std::pow(2, semis);
        ch << QString("asetrate=48000*%1,aresample=48000,atempo=%2").arg(F(rate, 5)).arg(F(1.0 / rate, 5));
    }
    auto [bOn, bAmt] = g("bass");
    if (bOn) ch << ("bass=g=" + F(2 + bAmt * 16, 2) + ":f=200");
    auto [tOn, tAmt] = g("treble");
    if (tOn) ch << ("treble=g=" + F(2 + tAmt * 16, 2) + ":f=3200");
    auto [mOn, mAmt] = g("muffle");
    if (mOn) {
        double f = 3200 + (700 - 3200) * mAmt;
        ch << QString("highpass=f=%1,lowpass=f=%2").arg(F0(std::max(80.0, f * .35))).arg(F0(f));
    }
    auto [eOn, eAmt] = g("echo");
    if (eOn) ch << QString("aecho=0.85:0.88:%1:%2").arg(F0((.22 + eAmt * .18) * 1000)).arg(F(.15 + eAmt * .45, 2));
    auto [cOn, cAmt] = g("crush");
    if (cOn)
        ch << ("acrusher=level_in=1:level_out=1:bits=" + F(std::max(3.0, 12 - cAmt * 8), 1) +
               ":mode=log:aa=1,lowpass=f=" + F0(9000 + (2200 - 9000) * cAmt));
    return ch;
}

} // namespace rf
