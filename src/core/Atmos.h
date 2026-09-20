#pragma once
#include <QJsonObject>
#include <QStringList>
#include <utility>

namespace rf {

class Atmos {
public:
    static std::pair<QStringList, QStringList> layers(const QJsonObject& fxg, int W, int H, int fps, double t0);
    static QStringList audioFxChain(const QJsonObject& afx);
};

} // namespace rf
