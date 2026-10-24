#pragma once
#include <array>
#include <cmath>

namespace donation_logic {
inline bool validThresholds(const std::array<double,3>& a) {
    return std::isfinite(a[0]) && std::isfinite(a[1]) && std::isfinite(a[2]) &&
           a[0]>0 && a[1]>a[0] && a[2]>a[1];
}
inline int levelForAmount(double amount,const std::array<double,3>& a) {
    if(!std::isfinite(amount)||amount<=0||!validThresholds(a))return -1;
    if(amount>=a[2])return 2;
    if(amount>=a[1])return 1;
    if(amount>=a[0])return 0;
    return -1;
}
}
