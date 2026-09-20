#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include <thread>
#include <algorithm>

namespace rf {

class Catalogs {
public:
    static const QStringList& motions() {
        static const QStringList m = {
            "kenIn", "kenOut", "panLR", "panRL", "tiltUp", "tiltDown", "handheld",
            "punch", "drift", "float", "spiral", "dolly", "tilt3d", "slideIn", "focus"
        };
        return m;
    }

    // key -> ffmpeg xfade name; empty string = hard cut (means "none"/null in Python)
    static const QMap<QString, QString>& transMap() {
        static const QMap<QString, QString> t = {
            {"none", ""}, {"fade", "fade"}, {"zoom", "zoomin"}, {"whip", "slideleft"},
            {"push", "slideup"}, {"flash", "fadewhite"}, {"spin", "circleclose"},
            {"iris", "circleopen"}, {"blur", "dissolve"}, {"leak", "fadewhite"},
        };
        return t;
    }

    static const QMap<QString, QString>& looks() {
        static const QMap<QString, QString> l = {
            {"none", ""},
            {"warm", "eq=saturation=1.14:contrast=1.04,colorbalance=rs=0.10:bs=-0.10:rm=0.07:bm=-0.07"},
            {"cool", "eq=saturation=1.06:contrast=1.04,colorbalance=rs=-0.09:bs=0.13:bm=0.06"},
            {"cine", "colorbalance=rs=-0.12:bs=0.16:rh=0.14:bh=-0.10,eq=contrast=1.10:saturation=1.12"},
            {"bw", "hue=s=0,eq=contrast=1.10:gamma=1.02"},
            {"vintage", "eq=saturation=0.55:contrast=0.94:gamma=1.06,colorbalance=rm=0.12:gm=0.05:bm=-0.08:bs=0.05"},
            {"vivid", "eq=saturation=1.45:contrast=1.14:gamma=0.98"},
            {"moody", "eq=saturation=0.72:contrast=1.18:brightness=-0.05,colorbalance=bs=0.12:bh=-0.05"},
        };
        return l;
    }

    static constexpr double kOversample = 1.45;
    static constexpr int kBlurDiv = 8;
    static constexpr int kMinClipsPerChunk = 2;

    static int cpuCount() {
        unsigned n = std::thread::hardware_concurrency();
        return static_cast<int>(std::max(1u, n));
    }
};

} // namespace rf
