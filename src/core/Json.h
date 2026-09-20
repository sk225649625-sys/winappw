#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <cmath>

namespace rf {

// editor ka project JSON Python dict jaisa loose/dynamic hai (keys missing,
// null, ya kabhi string jahan number chahiye). Ye helpers wahi tolerant
// access dete hain -- Qt ki apni QJsonObject use karte hain, koi extra
// JSON library nahi chahiye, aur kahin network nahi chhuta.
class J {
public:
    static double num(const QJsonValue& v, double def = 0.0) {
        if (v.isUndefined() || v.isNull()) return def;
        if (v.isDouble()) {
            double d = v.toDouble();
            if (std::isnan(d) || std::isinf(d)) return def;
            return d;
        }
        if (v.isString()) {
            bool ok = false;
            double d = v.toString().toDouble(&ok);
            return ok ? d : def;
        }
        if (v.isBool()) return v.toBool() ? 1.0 : 0.0;
        return def;
    }
    static double num(const QJsonObject& o, const QString& key, double def = 0.0) {
        return num(o.value(key), def);
    }
    static QString str(const QJsonObject& o, const QString& key, const QString& def = QString()) {
        auto v = o.value(key);
        if (v.isUndefined() || v.isNull()) return def;
        if (v.isString()) return v.toString();
        return def;
    }
    static bool boolean(const QJsonValue& v, bool def = false) {
        if (v.isUndefined() || v.isNull()) return def;
        if (v.isBool()) return v.toBool();
        if (v.isDouble()) return v.toDouble() != 0;
        if (v.isString()) return !v.toString().isEmpty();
        return def;
    }
    static bool boolean(const QJsonObject& o, const QString& key, bool def = false) {
        return boolean(o.value(key), def);
    }
    static QJsonArray arr(const QJsonObject& o, const QString& key) {
        auto v = o.value(key);
        return v.isArray() ? v.toArray() : QJsonArray();
    }
    static QJsonObject obj(const QJsonValue& v) {
        return v.isObject() ? v.toObject() : QJsonObject();
    }

    /// Single-quoted ffmpeg filter value ke andar daalne ke liye (esc_q).
    static QString escQ(const QString& s) {
        QString r = s;
        r.replace("\\", "/");
        r.replace("'", QString::fromUtf8(u8"\u2019"));
        return r;
    }
};

} // namespace rf
