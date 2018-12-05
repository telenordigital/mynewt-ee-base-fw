#ifndef _LOAD_SWITCH_H_
#define _LOAD_SWITCH_H_

#define RDM_BOOST_ENABLE_PIN 30
#define HIGH_SIDE_SWITCH_ENABLE_PIN 31

void init_load_switch();    
void powerOn();
void powerOff();

#endif