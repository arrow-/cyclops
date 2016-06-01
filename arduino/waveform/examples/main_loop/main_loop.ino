#include <Waveform.h>
#include <Source.h>
#include <TimerOne.h>

#include <Wire.h>
#include <SPI.h>
#include <nbSPI.h>
#include <Cyclops.h>
#include <RPC_defs.h>

uint16_t vd1[]  = {0x80, 0x100, 0x180, 0x300, 0x180, 0x100, 0x80, 0x3e};
double   htd1[] = {5e5, 3e4, 5e5, 4e5,1e5, 21.32e4, 7e5, 8e5}; // delay in us
storedSource s1(vd1, htd1, 8);
uint16_t vd2[]  = {0x80, 0x100, 0x180, 0x300, 0x180, 0x100};
double   htd2[] = {5e5, 5e5, 5e5, 5e5, 5e5, 5e5}; // delay in us
storedSource s2(vd2, htd2, 6);
uint16_t vd3[]  = {0x80, 0x100, 0x180, 0x300, 0x180, 0x100, 0x80, 0x3e, 0x423, 0xe2a, 0x999};
double   htd3[] = {5e5, 3e5, 5e5, 31e4, 5e5, 3e5, 5.98e5, 3e5, 2.34e4, 54.12e4, 9.992e5}; // delay in us
storedSource s3(vd3, htd3, 11);

Cyclops ch0(CH0);
Cyclops ch1(CH1);
Cyclops ch2(CH2);

void setup(){
  waveformList.setWaveform(&ch0, &s1, s1.opMode);
  waveformList.setWaveform(&ch1, &s2, s2.opMode);
  waveformList.setWaveform(&ch2, &s3, s3.opMode);
  
  // Serial.begin(57600);
  initSPI();
  // blocking call
  double initial_delta = waveformList.initialPrep();
  Timer1.initialize(initial_delta);
  // this call can be hidden, but it helps in understanding the workings
  Timer1.attachInterrupt(cyclops_timer_ISR);
}

void loop(){
  waveformList.process();
  // over_current_protection();
  // process the RPC tasks
  // Poll Serial Rx buffer
}
