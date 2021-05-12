const int scaleStableInterval = 500;   //稳定状态监测间隔（毫秒）
int oledPrintInterval = 0;     //oled刷新间隔（毫秒）
const int serialPrintInterval = 100;  //称重输出间隔（毫秒）
unsigned long t = 0;                  //最后一次重量输出打点
unsigned long oledRefreshMarker = 0;   //最后一次oled刷新打点
float aWeight = 0;          //稳定状态比对值（g）
float aWeightDiff = 0.05;    //稳定停止波动值（g）
float atWeight = 0;         //自动归零比对值（g）
float atWeightDiff = 0.3;   //自动归零波动值（g）
float asWeight = 0;         //下液停止比对值（g）
float asWeightDiff = 0.1;   //下液停止波动值（g）
float rawWeight = 0.0;      //原始读出值（g）

//电子秤外壳对屏幕的遮盖像素补偿
int Margin_Top = 5;
int Margin_Bottom = 6;
int Margin_Left = 0;
int Margin_Right = 0;

//Water Control
const int PIN_WATER = 6;
#define LEDPWR0 A0
#define LEDPWR1 A1
#define LEDWTR0 A2
#define LEDWTR1 A3

//HX711模数转换初始化
#include <HX711_ADC.h>
const int HX711_dout = 2;  //mcu > HX711 dout pin
const int HX711_sck = 3;   //mcu > HX711 sck pin
HX711_ADC scale(HX711_dout, HX711_sck);//HX711 constructor
//电子秤校准参数
const float calibrationValue = 1118.68; //白色39元称参数
//const float calibrationValue = 1028.52; //黑色珠宝称参数
//const float calibrationValue = 472.01;  //实验亚克力称参数

//按钮初始化
#include <AceButton.h>
using namespace ace_button;
const int PIN_BUTTON_TARE = 10;
const int PIN_BUTTON_START = 12;
#ifdef ESP32
// Different ESP32 boards use different pins
const int PIN_LED = 2;
#else
const int PIN_LED = LED_BUILTIN;
#endif
const int LED_ON = HIGH;
const int LED_OFF = LOW;
ButtonConfig config1;
ButtonConfig config2;
AceButton button1(&config1);
AceButton button2(&config2);
void handleEvent1(AceButton*, uint8_t, uint8_t);
void handleEvent2(AceButton*, uint8_t, uint8_t);
void button_init() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON_TARE, INPUT_PULLUP);
  pinMode(PIN_BUTTON_START, INPUT_PULLUP);
  button1.init(PIN_BUTTON_TARE);
  button2.init(PIN_BUTTON_START);
  config1.setEventHandler(handleEvent1);
  config1.setFeature(ButtonConfig::kFeatureClick);
  config1.setDebounceDelay(10);
  config2.setEventHandler(handleEvent2);
  config2.setFeature(ButtonConfig::kFeatureClick);
  config2.setDebounceDelay(10);
}

void handleEvent1(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  Serial.print(F("handleEvent1(): pin: "));
  Serial.print(button->getPin());
  Serial.print(F("; eventType: "));
  Serial.print(eventType);
  Serial.print(F("; buttonState: "));
  Serial.println(buttonState);
  switch (eventType) {
    case AceButton::kEventPressed:
      digitalWrite(PIN_LED, LED_ON);
      break;
    case AceButton::kEventReleased:
      digitalWrite(PIN_LED, LED_OFF);
      Serial.println(F("Button 1 clicked! Tare"));
      scale.tare();
      break;
    case AceButton::kEventLongReleased:
      break;
  }
}

void handleEvent2(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  // Print out a message for all events, for both buttons.
  Serial.print(F("handleEvent2(): pin: "));
  Serial.print(button->getPin());
  Serial.print(F("; eventType: "));
  Serial.print(eventType);
  Serial.print(F("; buttonState: "));
  Serial.println(buttonState);
  switch (eventType) {
    case AceButton::kEventPressed:
      digitalWrite(PIN_LED, LED_ON);
      break;
    case AceButton::kEventReleased:
      digitalWrite(PIN_LED, LED_OFF);
      if (!isWaterRunning()) {
        Serial.println(F("Button 2 clicked! Water Go!"));
        waterGo();
      }
      else {
        Serial.println(F("Button 2 clicked! Water Stop!"));
        waterStop();
      }
      break;
    case AceButton::kEventLongReleased:
      break;
  }
}

//显示屏初始化 https://github.com/olikraus/u8g2/wiki/u8g2reference
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
//U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);
//文本对齐 AC居中 AR右对齐 AL左对齐 T为要显示的文本
#define LCDWidth  u8g2.getDisplayWidth()
#define LCDHeight u8g2.getDisplayHeight()
#define AC(T)     ((LCDWidth - (u8g2.getStrWidth(T))) / 2)
#define AR(T)     (LCDWidth -  u8g2.getStrWidth(T))
#define AL        0
//设置字体 https://github.com/olikraus/u8g2/wiki/fntlistall
#define FONT_S u8g2_font_Pixellari_tu
#define FONT_L u8g2_font_VCR_OSD_tu
int FONT_L_HEIGHT;
int FONT_S_HEIGHT;

//自定义trim消除空格
char *ltrim(char *s) {
  while (isspace(*s)) s++;
  return s;
}

char *rtrim(char *s) {
  char* back = s + strlen(s);
  while (isspace(*--back));
  *(back + 1) = '\0';
  return s;
}

char *trim(char *s) {
  return rtrim(ltrim(s));
}

void btnClick(int inputPin)
{
  pinMode(inputPin, OUTPUT);
  delay(100);
  pinMode(inputPin, INPUT);
  delay(100);
}

void btnDoubleClick(int inputPin)
{
  pinMode(inputPin, OUTPUT);
  delay(100);
  pinMode(inputPin, INPUT);
  delay(100);
  pinMode(inputPin, OUTPUT);
  delay(100);
  pinMode(inputPin, INPUT);
  delay(100);
}

bool isPowerOn()
{
  float result = 0;
  for (int i = 0; i < 20; i++) {
    int v0 = analogRead(LEDPWR0);
    float volt0 = v0 * (5.0 / 1024.0);
    int v1 = analogRead(LEDPWR1);
    float volt1 = v1 * (5.0 / 1024.0);
    result =  result + abs(volt0 - volt1);
    delay(10);
  }
  if (result > 5) {
    Serial.println("Power On");
    return true;
  }
  else
    Serial.println("Power Off");
  return false;
}

bool isWaterRunning()
{
  if (isPowerOn()) {
    float result = 0;
    for (int i = 0; i < 20; i++) {
      int v0 = analogRead(LEDWTR0);
      float volt0 = v0 * (5.0 / 1024.0);
      int v1 = analogRead(LEDWTR1);
      float volt1 = v1 * (5.0 / 1024.0);
      result =  result + abs(volt0 - volt1);
      delay(10);
      //Serial.println(result);
    }
    if (result > 5) {
      Serial.println("Running");
      return true;
    }
    else
      Serial.println("Not Running");
    return false;
  }
  else {
    Serial.println("Power is off, not to say water");
    return false;
  }
}

void waterStop() {
  if (isWaterRunning()) {
    btnClick(PIN_WATER);
    Serial.println("water stop");
  }
}

void waterGo() {
  if (!isWaterRunning()) {
    btnDoubleClick(PIN_WATER);
    Serial.println("water go");
  }
}
void setup() {
  delay(1000); //有些单片机会重启两次
  Serial.begin(115200);
  while (!Serial); //等待串口就绪
  Serial.println(F("soso Auto Water"));
  button_init();
  u8g2.begin();
  u8g2.setFont(FONT_L);
  FONT_L_HEIGHT = u8g2.getMaxCharHeight();
  u8g2.setFont(FONT_S);
  FONT_S_HEIGHT = u8g2.getMaxCharHeight();

  scale.begin();
  unsigned long stabilizingtime = 2000;  //去皮时间(毫秒)，增加可以提高去皮精确度
  boolean _tare = true;                  //电子秤初始化去皮，如果不想去皮则设为false
  scale.start(stabilizingtime, _tare);
  scale.setCalFactor(calibrationValue);  //设定校准值
  scale.setSamplesInUse(4);               //设定采样窗口长度
  Serial.print(F("scale calibrated, sps="));
  Serial.println(scale.getSPS());
  button1.check();
  button2.check();

  Serial.println(F("setup done"));
}

void loop() {
 
  button1.check();
  button2.check();
  static boolean newDataReady = 0;
  static boolean scaleStable = 0;
  char coffee[10];
  if (scale.update()) newDataReady = true;
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      rawWeight = scale.getData();
      newDataReady = 0;
      t = millis();
      //-0.0 -> 0.0 正负符号稳定
      if (rawWeight > -0.15 && rawWeight < 0)
        rawWeight = 0.0;
      dtostrf(rawWeight, 7, 1, coffee);
    }
    if (rawWeight > 1170)
      waterStop();
    if (millis() > oledRefreshMarker + oledPrintInterval)
    {
      //达到设定的oled刷新频率后进行刷新
      oledRefreshMarker = millis();
      int x = 3;
      int y = 5;
      if (rawWeight <= -1999.9)
        sprintf(coffee, "-1999.9");
      char stopwatch_elapsed[30];
      String result_coffee[30] = coffee;

      u8g2.firstPage();
      do {
        u8g2.setFont(FONT_L);
        y =  FONT_S_HEIGHT + FONT_L_HEIGHT + Margin_Top;
        u8g2.drawStr(AR(trim(coffee)), y, trim(coffee));

        u8g2.setFont(FONT_S);
        y = FONT_S_HEIGHT + Margin_Top;
        u8g2.drawStr(AR("WEIGHT"), y, "WEIGHT");

      } while ( u8g2.nextPage() );
    }
  }
}
