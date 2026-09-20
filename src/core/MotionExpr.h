#pragma once
#include <QJsonObject>
#include <QString>
#include <optional>

namespace rf {

struct MotionResult {
    QString z, tx, ty;
    std::optional<QString> rot;
};

class MotionExpr {
public:
    static QString nzx(const QString& inner);
    static MotionResult compute(const QJsonObject& clip, int W, int H, int fps, int clipFrames, double off = 0.0);
    static std::optional<QString> beatMult(const QJsonObject& fxg, int fps, double off);
};

} // namespace rf
