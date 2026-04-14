#include <MIDIUSB.h>
#include <avr/wdt.h>

/*
micro.build.pid=0x8039
micro.build.usb_product="MEMI"
*/

// ------------------ 핀 설정 ------------------
const int gatePin1 = 5;
const int clockPin1 = 6;
const int gatePin2 = 7;
const int clockPin2 = 8;

const int clockLedPin = 10;  // PWM
const int gateLedPin = 3;    // PWM
const int gateLedPin2 = 9;   // PWM

float clockLedBrightness = 0;
float gateLed1Brightness = 0;
float gateLed2Brightness = 0;

const int releaseTime = 130;  // 릴리즈 지속 시간
const unsigned long pulseWidth = 10000; // 10ms (micros)

bool gateState1 = false;
bool gateState2 = false;
unsigned long lastNoteTime = 0;
unsigned long lastClockTime1 = 0;
unsigned long lastClockTime2 = 0;

// --- [추가] 게이트 루핑 끊김 방지용 (2ms) ---
unsigned long offTimer1 = 0;
bool pendingOff1 = false;
unsigned long offTimer2 = 0;
bool pendingOff2 = false;
const int deglitchTime = 2; // 요청하신 2ms 설정

int dinClockCounter = 0;
int noteCounter = 0;
#define NOTE_COUNTER_THRESHOLD 16
const unsigned long resetThreshold = 5000; 

// ------------------ 애니메이션 및 MIDI 전송 설정 ------------------
int animStep = 0;
int animSubStep = 0;
float animValue = 0;
const float animFadeInSpeed = 1;
const float animFadeOutSpeed = 0.3;
const int pauseDuration = 1500;
unsigned long pauseStartTime = 0;

void sendUSBMIDI(uint8_t cin, uint8_t d0, uint8_t d1, uint8_t d2) {
  midiEventPacket_t packet = { cin, d0, d1, d2 };
  MidiUSB.sendMIDI(packet);
}

// ------------------ LED / CV / Gate 제어 핸들러 (수정됨) ------------------
void handleNoteOn1(byte note) {
  lastNoteTime = millis();
  digitalWrite(clockPin1, HIGH);
  lastClockTime1 = micros();
  noteCounter++;
  if (noteCounter % 4 == 1) clockLedBrightness = 255;
  if (noteCounter >= NOTE_COUNTER_THRESHOLD) noteCounter %= 4;
}

void handleNoteOnGate1() {
  lastNoteTime = millis();
  pendingOff1 = false;       // 꺼짐 예약 취소
  gateState1 = true;
  digitalWrite(gatePin1, HIGH); 
  gateLed1Brightness = 255;
}

void handleNoteOn2(byte note) {
  lastNoteTime = millis();
  digitalWrite(clockPin2, HIGH);
  lastClockTime2 = micros();
  noteCounter++;
  if (noteCounter % 4 == 1) clockLedBrightness = 255;
  if (noteCounter >= NOTE_COUNTER_THRESHOLD) noteCounter %= 4;
}

void handleNoteOnGate2() {
  lastNoteTime = millis();
  pendingOff2 = false;       // 꺼짐 예약 취소
  gateState2 = true;
  digitalWrite(gatePin2, HIGH); 
  gateLed2Brightness = 255;
}

// 즉시 끄지 않고 예약만 함
void handleNoteOffGate1() { 
  offTimer1 = millis(); 
  pendingOff1 = true; 
}
void handleNoteOffGate2() { 
  offTimer2 = millis(); 
  pendingOff2 = true; 
}

// ------------------ DIN MIDI 처리 ------------------
const uint16_t DIN_SYSEX_BUFFER_SIZE = 256;
uint8_t dinSysExBuffer[DIN_SYSEX_BUFFER_SIZE];
uint16_t dinSysExIndex = 0;
bool inSysExDIN = false;
uint8_t dinMidiBuffer[3] = { 0, 0, 0 };
uint8_t dinByteIndex = 0;
uint8_t dinExpectedBytes = 0;
uint8_t dinRunningStatus = 0;

// ------------------ MIDI 처리 로직 (기존 유지) ------------------
void processSysExDIN() {
  uint16_t total = dinSysExIndex;
  uint16_t idx = 0;
  while (idx < total) {
    uint8_t data[3] = { 0, 0, 0 }; uint8_t count = 0;
    for (uint8_t k = 0; k < 3; k++) { if (idx < total) { data[k] = dinSysExBuffer[idx++]; count++; } }
    uint8_t cin = (idx < total) ? 0x4 : (count == 1 ? 0x5 : (count == 2 ? 0x6 : 0x7));
    sendUSBMIDI(cin, data[0], data[1], data[2]);
  }
}

void processDINByte(uint8_t inByte) {
  if (inByte >= 0xF8 && inByte != 0xF7) {
    sendUSBMIDI(0x0F, inByte, 0, 0); MidiUSB.flush();
    if (inByte == 0xF8) {
      dinClockCounter++; lastNoteTime = millis();
      if (dinClockCounter % 6 == 0) {
        digitalWrite(clockPin1, HIGH); digitalWrite(clockPin2, HIGH);
        lastClockTime1 = micros(); lastClockTime2 = micros();
      }
      if (dinClockCounter >= 24) { clockLedBrightness = 255; dinClockCounter = 0; }
    } 
    else if (inByte == 0xFA || inByte == 0xFB) {
      dinClockCounter = 0; noteCounter = 0; clockLedBrightness = 255;
      digitalWrite(clockPin1, HIGH); digitalWrite(clockPin2, HIGH);
      lastClockTime1 = micros(); lastClockTime2 = micros(); lastNoteTime = millis();
    }
    return;
  }
  // (중략된 SysEx/일반 메시지 파싱 - 기존과 동일)
  if (inSysExDIN) {
    if (dinSysExIndex < DIN_SYSEX_BUFFER_SIZE) dinSysExBuffer[dinSysExIndex++] = inByte;
    if (inByte == 0xF7) { processSysExDIN(); dinSysExIndex = 0; inSysExDIN = false; }
    return;
  }
  if (inByte == 0xF0) { inSysExDIN = true; dinSysExIndex = 0; dinSysExBuffer[dinSysExIndex++] = inByte; return; }
  if (inByte & 0x80) {
    if (inByte >= 0xF0) {
      dinRunningStatus = 0; dinMidiBuffer[0] = inByte; dinByteIndex = 1;
      if (inByte == 0xF1 || inByte == 0xF3) dinExpectedBytes = 1;
      else if (inByte == 0xF2) dinExpectedBytes = 2; else dinExpectedBytes = 0;
      if (dinExpectedBytes == 0) { sendUSBMIDI((inByte == 0xF6 ? 0x5 : 0xF), dinMidiBuffer[0], 0, 0); dinByteIndex = 0; }
    } else {
      dinRunningStatus = inByte; dinMidiBuffer[0] = inByte; dinByteIndex = 1;
      dinExpectedBytes = ((inByte & 0xF0) == 0xC0 || (inByte & 0xF0) == 0xD0) ? 1 : 2;
    }
  } else {
    if (dinByteIndex == 0 && dinRunningStatus != 0) {
      dinMidiBuffer[0] = dinRunningStatus; dinByteIndex = 1;
      dinExpectedBytes = ((dinRunningStatus & 0xF0) == 0xC0 || (dinRunningStatus & 0xF0) == 0xD0) ? 1 : 2;
    }
    if (dinByteIndex > 0 && dinByteIndex <= dinExpectedBytes) dinMidiBuffer[dinByteIndex++] = inByte;
  }
  if (dinByteIndex == dinExpectedBytes + 1) {
    uint8_t status = dinMidiBuffer[0]; uint8_t command = status & 0xF0; uint8_t channel = status & 0x0F;
    if (channel == 15) {
      if (command == 0x90 && dinMidiBuffer[2] > 0) {
        if (dinMidiBuffer[1] == 36) handleNoteOn1(dinMidiBuffer[1]);
        else if (dinMidiBuffer[1] == 38) handleNoteOnGate1();
        else if (dinMidiBuffer[1] == 48) handleNoteOn2(dinMidiBuffer[1]);
        else if (dinMidiBuffer[1] == 50) handleNoteOnGate2();
      } else if (command == 0x80 || (command == 0x90 && dinMidiBuffer[2] == 0)) {
        if (dinMidiBuffer[1] == 38) handleNoteOffGate1();
        else if (dinMidiBuffer[1] == 50) handleNoteOffGate2();
      }
    }
    sendUSBMIDI((status < 0xF0 ? status >> 4 : 0x3), dinMidiBuffer[0], dinMidiBuffer[1], dinMidiBuffer[2]);
    dinByteIndex = 0;
  }
}

void readDINMIDI() { while (Serial1.available() > 0) processDINByte(Serial1.read()); }

void readMIDI() {
  midiEventPacket_t rx;
  while ((rx = MidiUSB.read()).header != 0) {
    if (rx.byte1 >= 0xF8) { Serial1.write(rx.byte1); } 
    else {
      byte channel = rx.byte1 & 0x0F;
      if (channel != 15) { Serial1.write(rx.byte1); Serial1.write(rx.byte2); Serial1.write(rx.byte3); }
    }
    byte command = rx.byte1 & 0xF0; byte channel = rx.byte1 & 0x0F;
    if (channel == 15) {
      if (command == 0x90 && rx.byte3 > 0) {
        if (rx.byte2 == 36) handleNoteOn1(rx.byte2);
        else if (rx.byte2 == 38) handleNoteOnGate1();
        else if (rx.byte2 == 48) handleNoteOn2(rx.byte2);
        else if (rx.byte2 == 50) handleNoteOnGate2();
      } else if (command == 0x80 || (command == 0x90 && rx.byte3 == 0)) {
        if (rx.byte2 == 38) handleNoteOffGate1();
        else if (rx.byte2 == 50) handleNoteOffGate2();
      }
    }
  }
}

// ------------------ 루프 업데이트 (지연 로직 통합) ------------------
void updateClock() {
  if (digitalRead(clockPin1) == HIGH && micros() - lastClockTime1 > pulseWidth) digitalWrite(clockPin1, LOW);
  if (digitalRead(clockPin2) == HIGH && micros() - lastClockTime2 > pulseWidth) digitalWrite(clockPin2, LOW);

  unsigned long now = millis();

  // --- [핵심] Gate 지연 Off 처리 (2ms) ---
  if (pendingOff1 && (now - offTimer1 >= deglitchTime)) {
    digitalWrite(gatePin1, LOW);
    gateState1 = false; // 핀이 꺼질 때 비로소 LED 페이드 시작
    pendingOff1 = false;
  }
  if (pendingOff2 && (now - offTimer2 >= deglitchTime)) {
    digitalWrite(gatePin2, LOW);
    gateState2 = false;
    pendingOff2 = false;
  }

  static unsigned long lastUpd = 0;
  if (now - lastUpd >= 1) {
    lastUpd = now;
    if (now - lastNoteTime <= resetThreshold) {
      animValue = 0; animSubStep = 0;
      float drop = 255.0 / releaseTime;
      if (clockLedBrightness > 0) clockLedBrightness -= drop;
      if (!gateState1 && gateLed1Brightness > 0) gateLed1Brightness -= drop;
      if (!gateState2 && gateLed2Brightness > 0) gateLed2Brightness -= drop;
    } else {
      dinClockCounter = 0; noteCounter = 0;
      if (animSubStep == 0) { animValue += animFadeInSpeed; if (animValue >= 255) { animValue = 255; animSubStep = 1; } }
      else if (animSubStep == 1) { animValue -= animFadeOutSpeed; if (animValue <= 0) { animValue = 0; animSubStep = 2; pauseStartTime = now; } }
      else if (animSubStep == 2) { if (now - pauseStartTime >= pauseDuration) { animSubStep = 0; animStep = (animStep + 1) % 3; } }
      clockLedBrightness = (animStep == 0) ? animValue : 0;
      gateLed1Brightness = (animStep == 1 && !gateState1) ? animValue : (gateState1 ? 255 : 0);
      gateLed2Brightness = (animStep == 2 && !gateState2) ? animValue : (gateState2 ? 255 : 0);
    }
  }
  analogWrite(clockLedPin, (int)constrain(clockLedBrightness, 0, 255));
  analogWrite(gateLedPin, (int)constrain(gateLed1Brightness, 0, 255));
  analogWrite(gateLedPin2, (int)constrain(gateLed2Brightness, 0, 255));
}

void setup() {
  wdt_disable();
  delay(1000);
  wdt_enable(WDTO_2S);
  pinMode(gatePin1, OUTPUT); pinMode(clockPin1, OUTPUT);
  pinMode(gatePin2, OUTPUT); pinMode(clockPin2, OUTPUT);
  pinMode(clockLedPin, OUTPUT); pinMode(gateLedPin, OUTPUT); pinMode(gateLedPin2, OUTPUT);
  int checkPins[] = { clockLedPin, gateLedPin, gateLedPin2 };
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < 3; i++) { digitalWrite(checkPins[i], HIGH); delay(40); digitalWrite(checkPins[i], LOW); delay(40); }
  }
  Serial1.begin(31250);
}

void loop() {
  wdt_reset();
  readMIDI();
  readDINMIDI();
  updateClock();
  MidiUSB.flush();
}