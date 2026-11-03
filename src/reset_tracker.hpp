#pragma once
#include "model.hpp"
namespace uw {
struct ResetTracker {
 Window previous;int64_t notifiedDeadline=0;
 bool observe(const Window&current,int64_t now){
  if(!current.available)return false;
  bool due=previous.available&&previous.resets>0&&now>=previous.resets;
  bool rollover=due&&current.resets>previous.resets;
  bool replenished=due&&previous.remaining<99.5&&current.remaining>=99.5&&(current.resets==0||current.resets>=previous.resets);
  bool alert=(rollover||replenished)&&notifiedDeadline!=previous.resets;
  if(alert)notifiedDeadline=previous.resets;
  previous=current;return alert;
 }
};
}
