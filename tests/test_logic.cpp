#include "../src/donation_logic.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    using namespace donation_logic;
    const std::array<double,3> rub{100,500,2000};
    assert(validThresholds(rub));
    assert(levelForAmount(0,rub)==-1);
    assert(levelForAmount(99.99,rub)==-1);
    assert(levelForAmount(100,rub)==0);
    assert(levelForAmount(499.99,rub)==0);
    assert(levelForAmount(500,rub)==1);
    assert(levelForAmount(750,rub)==1);
    assert(levelForAmount(1999.99,rub)==1);
    assert(levelForAmount(2000,rub)==2);
    assert(levelForAmount(2500,rub)==2);
    assert(levelForAmount(std::numeric_limits<double>::quiet_NaN(),rub)==-1);
    assert(!validThresholds({100,100,2000}));
    assert(!validThresholds({0,500,2000}));
    assert(!validThresholds({2000,500,100}));
    assert(levelForAmount(1000,{100,100,2000})==-1);
    assert(levelForConvertedAmount(1,90,rub)==-1);
    assert(levelForConvertedAmount(2,90,rub)==0);
    assert(levelForConvertedAmount(1,100,rub)==0);
    assert(levelForConvertedAmount(1,500,rub)==1);
    assert(levelForConvertedAmount(1,2000,rub)==2);
    assert(levelForConvertedAmount(1,0,rub)==-1);
    assert(levelForConvertedAmount(1,-90,rub)==-1);
    assert(levelForConvertedAmount(1,std::numeric_limits<double>::quiet_NaN(),rub)==-1);
    assert(levelForConvertedAmount(1e308,1e308,rub)==-1);
    std::cout<<"Donation threshold logic tests: passed\n";
}
