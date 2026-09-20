#pragma once
#include <string>
#include <cstdio>

namespace rf {

// ffmpeg filter expressions ALWAYS '.' decimal point maangte hain. Agar
// Windows locale me comma-decimal ho (jaise Hindi/European locales), to
// normal sprintf galat output de sakta hai -- isliye ye helper hamesha
// "C" locale jaisa fixed '.' format deta hai (snprintf khud locale-safe
// hai jab tak koi setlocale(LC_NUMERIC,...) globally set na ho; hum wo
// kabhi set nahi karte, isliye ye default "C" hi rehta hai).
inline std::string fmt(double v, int decimals = 5) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return std::string(buf);
}

inline std::string fmtInt(long long v) {
    return std::to_string(v);
}

} // namespace rf
