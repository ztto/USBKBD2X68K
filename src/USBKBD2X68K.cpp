//---------------------------------------------------------------------
// USB/Keyboard(Japanese) to X68000 Keyboard And Mouse
// (Arduino Pro Mini 8MHz)
// ・USB_Host_Shield_2.0 (https://github.com/felis/USB_Host_Shield_2.0)
// ・MsTimer2 (http://playground.arduino.cc/Main/MsTimer2)
//
// minidin7P   --  Arduino Pro Mini
//  1 Vcc(+5V) --  Vcc(+5V)
//  2 MSDATA   --  18(A1)
//  3 KYRXD    --  2(D0)
//  4 KYTXD    --  1(D1)
//  5 KYREADY  --
//  6 KYRMT    --
//  7 GND      --  GND
//
// mouse control
//  mouse      --  19(A2)
//
// TLC59116 control
//  sda        --  A4(A4)
//  scl        --  A5(A5)
//  OUT0       --  KANA
//  OUT1       --  ROMA
//  OUT2       --  CODE
//  OUT3       --  CAPS
//  OUT4       --  INS
//  OUT5       --  HIRA
//  OUT6       --  ZEN
//---------------------------------------------------------------------
#include <SoftwareSerial.h>
#include <Wire.h>
#include <hidboot.h>
#include <usbhub.h>
#include <MsTimer2.h>
#include "keymap.h"

// DEBUG
#define DEBUG       0     // 0:デバッグ情報出力なし 1:デバッグ情報出力あり
#define LOBYTE(x) ((char*)(&(x)))[0]
#define HIBYTE(x) ((char*)(&(x)))[1]
// GPIO
#define MS_RX       14    // Mouse RX
#define MS_TX       15    // Mouse TX
#define MS_OFF      16    // Mouse Disable
// TLC59116
// I2C bus addresses
#define ADDRESS                        0b1100000  // A0-A3 gnd
// Control register
#define NO_AUTO_INCREMENT              0b00000000 // No auto-increment
#define AUTO_INCREMENT_ALL_REGISTERS   0b10000000 // Auto-increment for all registers
// TLC59116 registers
#define TLC59116_GRPPWM                0x12       // Group duty cycle control registers
#define TLC59116_LEDOUT0               0x14       // LED output state 0 registers
// LED output state
#define LED_OUTPUT_GROUP               0b11       // LEDOUT0-LEDOUT3

volatile uint8_t sCode = 0x00;          // スキャンコード
volatile uint8_t prevScode = 0x00;      // 前回スキャンコード
volatile int16_t sCodeCnt = 0;          // キーリピート回数
volatile int16_t delayTime = 500;       // キーリピート開始(初期値500ms)
volatile int16_t repeatTime = 110;      // キーリピート間隔(初期値110ms)
bool msOn = false;                      // マウス有効/無効
byte msData = 0;                        // マウスデータ
int16_t dxData = 0;                     // マウスデータ(X)
int16_t dyData = 0;                     // マウスデータ(Y)

SoftwareSerial msSerial(MS_RX, MS_TX);  // RX, TX

void sendRepeat();
//
// HIDキーボード レポートパーサークラスの定義
//
class KbdRptParser : public KeyboardReportParser {
  protected:
    void OnControlKeysChanged(uint8_t before, uint8_t after);
    void OnKeyDown(uint8_t mod, uint8_t key);
    void OnKeyUp(uint8_t mod, uint8_t key);
    void OnKeyPressed(uint8_t key) {};
};

//
// HIDマウス レポートパーサークラスの定義
//
class MseRptParser : public MouseReportParser {
  protected:
    void OnMouseMove(MOUSEINFO *mi);
    void OnLeftButtonUp(MOUSEINFO *mi);
    void OnLeftButtonDown(MOUSEINFO *mi);
    void OnRightButtonUp(MOUSEINFO *mi);
    void OnRightButtonDown(MOUSEINFO *mi);
};

//
// X68000 makeコード送信(キー押し)
// 引数 key(IN) HID Usage ID
//
void sendKeyMake(uint8_t key) {
  // HID Usage ID から X68 スキャンコード に変換
  uint8_t code = 0;
  code = pgm_read_byte(&(keytable[key]));
  if (code == 0x00) {
    return;
  }
  sCodeCnt++;
  prevScode = code;
#if DEBUG
  Serial.print(F("UP2["));  Serial.print(F("key="));  Serial.print(key, HEX);
  Serial.print(F(" code="));  Serial.print(code, HEX);  Serial.println(F("]"));
#endif
  // X68キーの発行
  Serial.write(code);
  sCode = code;
  MsTimer2::set(delayTime, sendRepeat);
  MsTimer2::start();
}

//
// X68000 breakコード送信(キー離し)
// 引数 key(IN) HID Usage ID
//
void sendKeyBreak(uint8_t key) {
  // HID Usage ID から X68000 スキャンコード に変換
  uint8_t code = 0;
  code = pgm_read_byte(&(keytable[key]));
  if (code == 0x00) {
    return;
  }
  sCodeCnt--;
  if (prevScode == code) {
    sCode = 0x00;
  }
#if DEBUG
  Serial.print(F("DN ["));  Serial.print(F("key="));  Serial.print(key, HEX);
  Serial.print(F(" code="));  Serial.print(code, HEX);  Serial.println(F("]"));
#endif
  // X68キーの発行
  Serial.write(code | 0x80);
  if (sCodeCnt == 0) {
    MsTimer2::stop();
  }
}

//
// コントロールキー変更ハンドラ
// SHIFT、CTRL、ALT、GUI(Win)キーの処理を行う
// 引数 before : 変化前のコード USB Keyboard Reportの1バイト目
//      after  : 変化後のコード USB Keyboard Reportの1バイト目
//
void KbdRptParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;

  // 左Ctrlキー
  if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
    if (afterMod.bmLeftCtrl) {
      // 左Ctrltキーを押した
      sendKeyMake(0xe0);
    } else {
      // 左Ctrltキーを離した
      sendKeyBreak(0xe0);
    }
  }
  // 左Shiftキー
  if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
    if (afterMod.bmLeftShift) {
      // 左Shiftキーを押した
      sendKeyMake(0xe1);
    } else {
      // 左Shiftキーを離した
      sendKeyBreak(0xe1);
    }
  }
  // 左Altキー
  if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
    if (afterMod.bmLeftAlt) {
      // 左Altキーを押した ==> XF1
      sendKeyMake(0xe2);
    } else {
      // 左Altキーを離した ==> XF1
      sendKeyBreak(0xe2);
    }
  }
  // 左GUIキー(Winキー)
  if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
    if (afterMod.bmLeftGUI) {
      // 左GUIキーを押した
      sendKeyMake(0xe3);
    } else {
      // 左GUIキーを離した
      sendKeyBreak(0xe3);
    }
  }
  // 右Ctrlキー
  if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
    if (afterMod.bmRightCtrl) {
      // 右Ctrltキーを押した
      sendKeyMake(0xe4);
    } else {
      // 右Ctrltキーを離した
      sendKeyBreak(0xe4);
    }
  }
  // 右Shiftキー
  if (beforeMod.bmRightShift != afterMod.bmRightShift) {
    if (afterMod.bmRightShift) {
      // 右Shiftキーを押した
      sendKeyMake(0xe5);
    } else {
      // 右Shiftキーを離した
      sendKeyBreak(0xe5);
    }
  }
  // 右Altキー
  if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
    if (afterMod.bmRightAlt) {
      // 右Altキーを押した
      sendKeyMake(0xe6);
    } else {
      // 右Altキーを離した
      sendKeyBreak(0xe6);
    }
  }
  // 右GUIキー
  if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
    if (afterMod.bmRightGUI) {
      // 右GUIキーを押した
      sendKeyMake(0xe7);
    } else {
      // 右GUIキーを離した
      sendKeyBreak(0xe7);
    }
  }
}

//
// キー押しハンドラ
// 引数
//  mod : コントロールキー状態
//  key : HID Usage ID
//
void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key) {
#if DEBUG
  Serial.print(F("DN ["));  Serial.print(F("mod="));  Serial.print(mod, HEX);
  Serial.print(F(" key="));  Serial.print(key, HEX);  Serial.println(F("]"));
#endif
  // HID Usage ID から X68 スキャンコード に変換
  sendKeyMake(key);
}

//
// キー離し ハンドラ
// 引数
//  mod : コントロールキー状態
//  key : HID Usage ID
//
void KbdRptParser::OnKeyUp(uint8_t mod, uint8_t key) {
#if DEBUG
  Serial.print(F("UP ["));  Serial.print(F("mod="));  Serial.print(mod, HEX);
  Serial.print(F(" key="));  Serial.print(key, HEX);  Serial.println(F("]"));
#endif
  // HID Usage ID から X68 スキャンコード に変換
  sendKeyBreak(key);
}

//
// マウス移動 ハンドラ
// 引数
//  mi : マウス情報
//
void MseRptParser::OnMouseMove(MOUSEINFO *mi) {
#if DEBUG
  Serial.print("dx="); Serial.print(mi->dX, DEC);
  Serial.print(" dy="); Serial.print(mi->dY, DEC);
  Serial.print(" bmLeftButton="); Serial.print(mi->bmLeftButton, DEC);
  Serial.print(" bmRightButton="); Serial.print(mi->bmRightButton, DEC);
  Serial.print(" bmMiddleButton="); Serial.println(mi->bmMiddleButton, DEC);
#endif
  dxData += mi->dX;
  if (dxData > 255) dxData = 255;
  if (dxData < -255) dxData = -255;

  dyData += mi->dY;
  if (dyData > 255) dyData = 255;
  if (dyData < -255) dyData = -255;
}

//
// マウス左ボタン離し ハンドラ
// 引数
//  mi : マウス情報
//
void MseRptParser::OnLeftButtonUp(MOUSEINFO *mi) {
#if DEBUG
  Serial.println("L Butt Up");
#endif
  msData &= ~B00000001;
}

//
// マウス左ボタン押し ハンドラ
// 引数
//  mi : マウス情報
//
void MseRptParser::OnLeftButtonDown(MOUSEINFO *mi) {
#if DEBUG
  Serial.println("L Butt Dn");
#endif
  msData |= B00000001;
}

//
// マウス右ボタン離し ハンドラ
// 引数
//  mi : マウス情報
//
void MseRptParser::OnRightButtonUp(MOUSEINFO *mi) {
#if DEBUG
  Serial.println("R Butt Up");
#endif
  msData &= ~B00000010;
}

//
// マウス左ボタン押し ハンドラ
// 引数
//  mi : マウス情報
//
void MseRptParser::OnRightButtonDown(MOUSEINFO *mi) {
#if DEBUG
  Serial.println("R Butt Dn");
#endif
  msData |= B00000010;
}

//
// マウスデータ送信
//
void mouseSend() {
#if DEBUG
  if (msOn) {
    Serial.println(F("MOUSE ON"));
  } else {
    Serial.println(F("MOUSE OFF"));
  }
#endif
  if (msOn) {
    msSerial.write(msData);
    delayMicroseconds(250);

    msSerial.write((int8_t)(dxData / 2));
    delayMicroseconds(250);
    // 一度送信したらリセット
    dxData = 0;

    msSerial.write((int8_t)(dyData / 2));
    delayMicroseconds(250);
    // 一度送信したらリセット
    dyData = 0;
  }
}

//
// LED制御（点灯）
//
void ledLight(uint8_t kbdCtrl) {
  // キーボードのLED制御
  uint8_t kbdCtrlL = ~kbdCtrl;
  uint8_t registerVal = 0;
  uint8_t registerIncrement = LED_OUTPUT_GROUP;
  Wire.beginTransmission(ADDRESS);
  Wire.write(byte(AUTO_INCREMENT_ALL_REGISTERS + TLC59116_LEDOUT0));
  for (uint8_t i = 0; i < 8; i++) {
    if (kbdCtrlL & 0x01) {
      registerVal += registerIncrement;
    }
    kbdCtrlL >>= 1;
    if (registerIncrement == LED_OUTPUT_GROUP << 6) {
      Wire.write((byte)registerVal);
      registerVal = 0;
      registerIncrement = LED_OUTPUT_GROUP;
    } else {
      registerIncrement <<= 2;
    }
  }
  Wire.endTransmission();
}

//
// LED制御（明るさ）
//
void ledBright(uint8_t kbdCtrl) {
  // キーボードのLED明るさ制御
  uint8_t ledBrightness = 255;
  switch (kbdCtrl) {
    case 0b01010111:
      // 暗い
      ledBrightness = 123;
      break;
    case 0b01010110:
      // やや暗い
      ledBrightness = 167;
      break;
    case 0b01010101:
      // やや明るい
      ledBrightness = 211;
      break;
    case 0b01010100:
      // 明るい
      ledBrightness = 255;
      break;
  }
  Wire.beginTransmission(ADDRESS);
  // Group duty cycle control
  Wire.write(byte(NO_AUTO_INCREMENT + TLC59116_GRPPWM));
  Wire.write(byte(ledBrightness));
  Wire.endTransmission();
}

USB     Usb;
USBHub  Hub1(&Usb);

//HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);
//HIDBoot<USB_HID_PROTOCOL_MOUSE>    HidMouse(&Usb);
//HIDBoot<USB_HID_PROTOCOL_KEYBOARD | USB_HID_PROTOCOL_MOUSE>    HidComposite(&Usb);
HIDBoot<3>    HidComposite(&Usb);
HIDBoot<1>    HidKeyboard(&Usb);
HIDBoot<2>    HidMouse(&Usb);

KbdRptParser keyboardPrs;
MseRptParser msePrs;

// リピート処理(タイマー割り込み処理から呼ばれる)
void sendRepeat() {
  if (sCode != 0x00) {
    Serial.write(sCode);
  }
  MsTimer2::set(repeatTime, sendRepeat);
  MsTimer2::start();
}

//
// セットアップ
//
void setup() {
  // I2C初期化
  Wire.begin();
  // シリアル初期化
#if DEBUG
  Serial.begin(9600);
#else
  Serial.begin(2400);
#endif
  msSerial.begin(4800);
  while (!Serial);
#if DEBUG
  Serial.println("Self Test OK.");
#endif
  if (Usb.Init() == -1) {
#if DEBUG
    Serial.println(F("OSC did not start."));
#endif
    // Halt
    while (1);
  }
  // Inputモードでプルアップ抵抗を有効
  pinMode(MS_OFF, INPUT_PULLUP);
  if (digitalRead(MS_OFF) == HIGH) {
    msOn = true;
  } else {
    pinMode(MS_TX, INPUT);
  }
  // TLC59116初期化
  Wire.beginTransmission(ADDRESS);
  // control register
  Wire.write(byte(AUTO_INCREMENT_ALL_REGISTERS));
  // Mode1
  Wire.write(byte(0));
  // Mode2
  Wire.write(byte(0));
  // brightness control
  for (uint8_t i = 0; i < 16; i++) {
    Wire.write(byte(255));
  }
  // Group duty cycle control
  Wire.write(byte(255));
  // Group frequency
  Wire.write(byte(0));
  // LED output state 0-3
  for (uint8_t i = 0; i < 4; i++) {
    Wire.write(byte(0));
  }
  Wire.endTransmission();
  // Hid初期化
  HidComposite.SetReportParser(0, &keyboardPrs);
  HidComposite.SetReportParser(1, &msePrs);
  HidKeyboard.SetReportParser(0, &keyboardPrs);
  HidMouse.SetReportParser(0, &msePrs);
#if DEBUG
  Serial.println(F("Start."));
#endif
  // LED状態取得
  Serial.write(0xff);
}

//
// ループ
//
void loop() {
  Usb.Task();
  if (Serial.available()) {
    uint8_t kbdCtrl = Serial.read();
    if ((kbdCtrl >> 7) == 0b1) {
      // LED LIGHT
      ledLight(kbdCtrl);
    } else {
      switch (kbdCtrl >> 6) {
        case 0b00:
          // TV CONTROL
          break;
        case 0b01:
          if ((kbdCtrl >> 4) == 0b0110) {
            // REP DELAY
            delayTime = (200 + (kbdCtrl & 0x0f) * 100);
            break;
          }
          if ((kbdCtrl >> 4) == 0b0111) {
            // REP TIME
            repeatTime = (30 + ((kbdCtrl & 0x0f) * (kbdCtrl & 0x0f)) * 5);
            break;
          }
          if ((kbdCtrl >> 3) == 0b01000) {
            // MOUSE CONTROL
            if ((kbdCtrl & 1) == 0) {
              // MOUSE CONTROL H -> L
              mouseSend();
            }
            break;
          }
          if ((kbdCtrl >> 3) == 0b01001) {
            // KEY EN
            break;
          }
          if ((kbdCtrl >> 2) == 0b010101) {
            // LED BRIGHT
            ledBright(kbdCtrl);
            break;
          }
          if ((kbdCtrl >> 2) == 0b010110) {
            // CTRL EN
            break;
          }
          if ((kbdCtrl >> 2) == 0b010111) {
            // OPT2 EN
            break;
          }
      }
    }
  }
}
