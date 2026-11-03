#include "../src/reset_tracker.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace uw;ResetTracker t;assert(!t.observe({true,1,100},90));assert(!t.observe({true,1,100},99));assert(t.observe({true,97,200},101));assert(!t.observe({true,97,200},102));assert(!t.observe({false,0,0},201));assert(t.observe({true,90,300},202));assert(!t.observe({true,100,300},250));ResetTracker a;assert(!a.observe({true,8,100},90));assert(a.observe({true,100,100},101));assert(!a.observe({true,100,100},102));ResetTracker b;assert(!b.observe({true,100,200},101));assert(!b.observe({true,99,300},120));ResetTracker c;c.previous={true,2,100};c.notifiedDeadline=100;assert(!c.observe({true,100,200},101));std::cout<<"Reset tracking cases passed\n";}
