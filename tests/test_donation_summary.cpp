#include "../src/donation_summary.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    sn::DonationSession session;
    assert(!session.observe("before",1,1,false));
    session.begin();
    assert(session.observe("DA:first",49,1,false));
    assert(session.observe("DP:second",1,80,false,true));
    assert(!session.observe("DA:first",49,1,false));
    assert(!session.observe("manual",500,1,true));
    assert(!session.observe("invalid",-1,1,false));
    assert(!session.observe("nan",std::numeric_limits<double>::quiet_NaN(),1,false));
    auto result=session.value();assert(result.count==2 && result.rub==129 && result.converted);
    session.end();assert(!session.observe("offline",5,1,false));
    assert(session.value().count==2);
    session.begin();assert(!session.observe("DA:first",49,1,false)); // Replay from previous session.
    assert(session.observe("new",10,0,false));
    result=session.value();assert(result.count==1 && result.unpriced==1 && result.rub==0);
    assert(session.observe("new-RUB",100,1,false));
    result=session.value();assert(result.count==2 && result.rub==100 && result.unpriced==1);
    std::cout<<"Donation session totals: below-threshold amounts, FX/unpriced, test exclusion, runtime replay deduplication and session reset passed\n";
}
