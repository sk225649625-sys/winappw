#include "MotionExpr.h"
#include "Json.h"
#include "Num.h"
#include <cmath>
#include <algorithm>

namespace rf {

static QString F(double v, int d = 5) { return QString::fromStdString(fmt(v, d)); }

QString MotionExpr::nzx(const QString& inner) {
    QString x = "(" + inner + ")";
    return "((sin(" + x + "*2.1)+0.6*sin(" + x + "*3.7+1.3)+0.3*sin(" + x + "*5.9+0.7))/1.9)";
}

MotionResult MotionExpr::compute(const QJsonObject& clip, int W, int H, int fps, int clipFrames, double off) {
    double I = std::max(0.0, std::min(1.5, J::num(clip, "intensity", .6)));
    double sd = J::num(clip, "seed", 0.0);
    QString m = J::str(clip, "motion", "kenIn");
    int nm1 = std::max(1, clipFrames - 1);

    QString T = "((on/" + F(fps, 6) + ")+" + F(off) + ")";
    QString Tr = "(t+" + F(off) + ")";
    QString P = "((on+" + F(off * fps, 3) + ")/" + QString::number(nm1) + ")";
    double sgn = std::fmod(sd, 2.0) < 1.0 ? 1.0 : -1.0;

    QString z = "1", tx = "0", ty = "0";
    std::optional<QString> rot;

    if (m == "kenIn") {
        double a = .07 + .2 * I;
        z = "(1.02+" + F(a) + "*" + P + ")";
        tx = "(" + F(std::sin(sd) * W * .01 * I) + "*(" + P + "-0.5))";
        ty = "(" + F(std::cos(sd) * H * .01 * I) + "*(" + P + "-0.5))";
    } else if (m == "kenOut") {
        z = "(1.02+" + F(.07 + .2 * I) + "*(1-" + P + "))";
    } else if (m == "panLR" || m == "panRL") {
        double A = .025 + .035 * I;
        z = F(1 + A * 3.2 + .02);
        double a0 = (m == "panLR") ? -A : A, a1 = (m == "panLR") ? A : -A;
        tx = "(" + F(a0 * W) + "+(" + F((a1 - a0) * W) + ")*" + P + ")";
        ty = "(" + F(H * .004 * I) + "*sin(" + T + "*0.8+" + F(sd) + "))";
    } else if (m == "tiltUp" || m == "tiltDown") {
        double A = .025 + .035 * I;
        z = F(1 + A * 3.2 + .02);
        double a0 = (m == "tiltUp") ? A : -A, a1 = (m == "tiltUp") ? -A : A;
        ty = "(" + F(a0 * H) + "+(" + F((a1 - a0) * H) + ")*" + P + ")";
        tx = "(" + F(W * .004 * I) + "*sin(" + T + "*0.7+" + F(sd) + "))";
    } else if (m == "handheld") {
        double k = .5 + 1.5 * I;
        z = "(1.07+" + F(.03 * I) + "+0.01*" + P + ")";
        tx = "(" + F(W * .006 * k) + "*" + nzx(T + "*0.9+" + F(sd)) + ")";
        ty = "(" + F(H * .006 * k) + "*" + nzx(T + "*1.1+" + F(sd * 2.3)) + ")";
        rot = "(" + F(.004 * k) + "*" + nzx(Tr + "*0.7+" + F(sd * 3.1)) + ")";
    } else if (m == "punch") {
        z = "(1.03+0.03*" + P + "+" + F(.05 + .16 * I) + "*exp(-" + T + "*5.5))";
        QString sh = "(exp(-" + T + "*9)*" + F(I, 4) + ")";
        tx = "(" + F(W * .01) + "*" + nzx(T + "*40+" + F(sd)) + "*" + sh + ")";
        ty = "(" + F(H * .01) + "*" + nzx(T + "*37+" + F(sd)) + "*" + sh + ")";
    } else if (m == "drift") {
        double R = .02 + .03 * I;
        double need = std::cos(R * 1.4) + std::max(W / (double)H, H / (double)W) * std::sin(R * 1.4);
        z = F(need * 1.03);
        double dur = std::max(.001, clipFrames / (double)fps);
        rot = "((" + F(-R) + "+(" + F(2 * R) + ")*min(1," + Tr + "/" + F(dur) + "))*" + F(sgn, 1) + ")";
        tx = "(" + F(W * .006) + "*sin(" + T + "*0.6))";
    } else if (m == "float") {
        z = "(1.06+0.05*" + P + ")";
        tx = "(" + F(W * .008) + "*sin((" + T + "+" + F(sd) + ")*0.6))";
        ty = "(" + F(H * .012) + "*sin((" + T + "+" + F(sd) + ")*1.1))";
        rot = "(" + F(.02 * (.5 + I)) + "*sin((" + Tr + "+" + F(sd) + ")*0.9))";
    } else if (m == "spiral") {
        QString e = "(1-pow(1-min(" + T + "/0.9,1),3))";
        QString er = "(1-pow(1-min(" + Tr + "/0.9,1),3))";
        z = "(1.02+0.04*" + P + "+(1-" + e + ")*" + F(.3 + .35 * I) + ")";
        rot = "((1-" + er + ")*" + F((.12 + .12 * I) * sgn) + ")";
    } else if (m == "dolly") {
        z = "(1.05+" + F(.06 + .16 * I) + "*" + P + ")";
        tx = "(" + F(W * .012 * (.5 + I)) + "*sin(" + T + "*0.7+" + F(sd) + "))";
        ty = "(" + F(H * .006) + "*cos(" + T + "*0.5+" + F(sd) + "))";
        rot = "(0.004*sin(" + Tr + "*0.4))";
    } else if (m == "tilt3d") {
        z = "(1.15+" + F(.05 * I) + "+0.03*" + P + ")";
        tx = "(" + F(W * .01) + "*sin((" + T + "+" + F(sd) + ")*0.4))";
        ty = "(" + F(H * .012 * (.5 + I)) + "*cos((" + T + "+" + F(sd) + ")*0.6))";
        rot = "(0.015*sin((" + Tr + "+" + F(sd) + ")*0.5))";
    } else if (m == "slideIn") {
        QString x = "min(" + T + "/0.85,1)";
        QString xr = "min(" + Tr + "/0.85,1)";
        QString e = "(1+2.70158*pow(" + x + "-1,3)+1.70158*pow(" + x + "-1,2))";
        QString er = "(1+2.70158*pow(" + xr + "-1,3)+1.70158*pow(" + xr + "-1,2))";
        z = "(1.30+0.08*" + P + ")";
        tx = "((1-" + e + ")*" + F(W * .22 * sgn) + ")";
        rot = "((1-" + er + ")*" + F(.05 * sgn) + ")";
    } else if (m == "focus") {
        z = "(1.02+0.14*(1-" + P + "))";
        ty = "(" + F(H * .004) + "*sin(" + T + "*0.6+" + F(sd) + "))";
    } else {
        z = "(1.02+" + F(.07 + .2 * I) + "*" + P + ")";
    }

    return { z, tx, ty, rot };
}

std::optional<QString> MotionExpr::beatMult(const QJsonObject& fxg, int fps, double off) {
    double bpm = J::num(fxg, "bpm", 0);
    double pulse = J::num(fxg, "pulse", .5);
    if (bpm <= 0 || pulse <= 0) return std::nullopt;
    double per = 60.0 / bpm;
    double ph = J::num(fxg, "phase", 0);
    return QString("(1+%1*exp(-mod((on/%2)+%3-%4+%5,%6)*9))")
        .arg(F(pulse * .07)).arg(fps).arg(F(off)).arg(F(ph)).arg(F(per * 8)).arg(F(per));
}

} // namespace rf
