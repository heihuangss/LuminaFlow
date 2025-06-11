 *
 * 智能 LED 控制器 - 支持 Blinker IoT
 *
 * 功能特性：
 * - 多种灯光效果：静态、呼吸、水流
 * - 两种颜色之间的渐变
 * - 所有效果的速度控制
 * - 继电器控制（用于附加设备）
 * - 连接时自动与 Blinker 应用同步
 *
 * 硬件配置：
 * - ESP8266/ESP32 开发板
 * - WS2812B LED 灯带（15 颗灯珠）
 * - 继电器模块
 *
 * 作者：[李承健]
 * 版本：1.1
 * GitHub: https://github.com/heihuangss/LuminaFlow
 *
 * MIT 许可证
 * 版权所有 (c) 2023 [李承健]
 *
 * 特此免费授予任何获得本软件及相关文档文件（以下简称“软件”）副本的人不受限制地处置软件的权利，包括但不限于使用、复制、修改、合并、发布、分发、转授许可和/或销售软件副本，以及准许接受软件提供的人员这样做，但须符合以下条件：
 *
 * 上述版权声明和本许可声明应包含在软件的所有副本或实质性部分中。
 *
 * 本软件按“原样”提供，不附有任何明示或暗示的担保，包括但不限于适销性担保、特定用途适用性和非侵权担保。在任何情况下，作者或版权持有人均不对因软件或使用或其他软件交易而引起、产生于或与之相关的任何索赔、损害赔偿或其他法律责任承担任何责任，无论是合同诉讼、侵权行为还是其他原因导致的。
 */


#define BLINKER_WIFI
#include <Blinker.h>
#include <Adafruit_NeoPixel.h>

const char AUTH[] = "your_device_key_here";         // Blinker设备密钥
const char SSID[] = "your_wifi_name";               // WiFi名称
const char PASSWORD[] = "your_wifi_password";       // WiFi密码
const uint8_t RELAY_PIN = 0;                        // 继电器控制引脚
const uint8_t LED_PIN = 2;                          // LED控制引脚
const uint8_t LED_COUNT = 15;                       // LED数量
const uint8_t MAX_BRIGHTNESS = 150;                 // 最大亮度值（保护LED）


Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 组件定义
BlinkerRGB RGB1("col-t3i");
BlinkerSlider BrightnessSlider("ran-8y7");
BlinkerButton ModeBtn("btn-gmq"), RelayBtn("btn-relay");
BlinkerButton GradientBtn("btn-834"), BreathSpeedBtn("btn-2da");
BlinkerButton FlowSpeedBtn("btn-mwk"), GradientSpeedBtn("btn-nw4");

// 状态变量
uint8_t mode = 0, breathSpeed = 0, flowSpeed = 0, gradientSpeed = 0;
bool relayState = false, gradientMode = false;
uint32_t color = 0xFF0000, secondaryColor = 0x0000FF;
int16_t breathValue = 128;
int8_t breathDirection = 1;
uint8_t flowPosition = 0, gradientStep = 0;
uint8_t brightness = 128;

void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  relayState = on;
  RelayBtn.icon(on ? "fas fa-toggle-on" : "fas fa-toggle-off");
  RelayBtn.color(on ? "#FF5252" : "#9E9E9E");
  RelayBtn.text(on ? "继电器:开" : "继电器:关");
  RelayBtn.print();
}

void updateLEDs() {
  strip.setBrightness(min(brightness, MAX_BRIGHTNESS));
  
  if (gradientMode && mode == 0) {
    for (int i = 0; i < LED_COUNT; i++) {
      float ratio = (gradientStep + i * (gradientSpeed + 1)) % LED_COUNT / (float)LED_COUNT;
      uint8_t r = ((color >> 16) & 0xFF) * (1 - ratio) + ((secondaryColor >> 16) & 0xFF) * ratio;
      uint8_t g = ((color >> 8) & 0xFF) * (1 - ratio) + ((secondaryColor >> 8) & 0xFF) * ratio;
      uint8_t b = (color & 0xFF) * (1 - ratio) + (secondaryColor & 0xFF) * ratio;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    gradientStep = (gradientStep + gradientSpeed + 1) % LED_COUNT;
  } else {
    strip.fill(color);
  }
  strip.show();
}

void handleEffects() {
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();
  
  if (mode == 1) {
    breathValue += breathDirection * (breathSpeed == 0 ? 1 : breathSpeed == 1 ? 2 : 4);
    if (breathValue >= 255 || breathValue <= 10) breathDirection *= -1;
    strip.setBrightness(breathValue);
    strip.fill(color);
    strip.show();
  } 
  else if (mode == 2 && now - lastUpdate > (flowSpeed == 0 ? 200 : flowSpeed == 1 ? 100 : 50)) {
    lastUpdate = now;
    strip.clear();
    for (int i = 0; i < 8; i++) 
      strip.setPixelColor((flowPosition + i) % LED_COUNT, color);
    strip.show();
    flowPosition = (flowPosition + 1) % LED_COUNT;
  }
}

void rgbCallback(uint8_t r, uint8_t g, uint8_t b, uint8_t) {
  color = strip.Color(r, g, b);
  updateLEDs();
}

void brightnessCallback(int32_t value) {
  brightness = map(value, 0, 100, 5, MAX_BRIGHTNESS);
  updateLEDs();
}

void modeCallback(const String &) {
  mode = (mode + 1) % 3;
  const char* icons[] = {"fas fa-lightbulb", "fas fa-wind", "fas fa-water"};
  const char* colors[] = {"#4CAF50", "#FF9800", "#2196F3"};
  const char* texts[] = {"静态模式", "呼吸模式", "流水模式"};
  
  ModeBtn.icon(icons[mode]);
  ModeBtn.color(colors[mode]);
  ModeBtn.text(texts[mode]);
  ModeBtn.print();
  
  updateLEDs();
}

void relayCallback(const String &) { setRelay(!relayState); }

void gradientCallback(const String &) {
  gradientMode = !gradientMode;
  GradientBtn.text(gradientMode ? "渐变色:开" : "渐变色:关");
  GradientBtn.icon(gradientMode ? "fas fa-fill-drip" : "fas fa-ban");
  GradientBtn.color(gradientMode ? "#9C27B0" : "#757575");
  GradientBtn.print();
  updateLEDs();
}

void updateSpeedBtn(BlinkerButton& btn, uint8_t speed, const char* type) {
  const char* icons[] = {"fas fa-tachometer-alt-slow", "fas fa-tachometer-alt-average", "fas fa-tachometer-alt-fast"};
  const char* colors[] = {"#FFEB3B", "#2196F3", "#F44336"};
  const char* texts[] = {"慢速", "中速", "快速"};
  
  String btnText = String(type) + ":" + texts[speed];
  btn.icon(icons[speed]);
  btn.color(colors[speed]);
  btn.text(btnText.c_str());
  btn.print();
}

void breathSpeedCallback(const String &) { 
  breathSpeed = (breathSpeed + 1) % 3;
  updateSpeedBtn(BreathSpeedBtn, breathSpeed, "呼吸");
}

void flowSpeedCallback(const String &) {
  flowSpeed = (flowSpeed + 1) % 3;
  updateSpeedBtn(FlowSpeedBtn, flowSpeed, "流水");
}

void gradientSpeedCallback(const String &) {
  gradientSpeed = (gradientSpeed + 1) % 3;
  updateSpeedBtn(GradientSpeedBtn, gradientSpeed, "渐变");
}

void heartbeat() {
  // 同步所有组件状态
  ModeBtn.print();
  RelayBtn.print();
  GradientBtn.print();
  updateSpeedBtn(BreathSpeedBtn, breathSpeed, "呼吸");
  updateSpeedBtn(FlowSpeedBtn, flowSpeed, "流水");
  updateSpeedBtn(GradientSpeedBtn, gradientSpeed, "渐变");
  
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  RGB1.print(r, g, b);
  
  int32_t brightnessPercent = map(brightness, 5, MAX_BRIGHTNESS, 0, 100);
  BrightnessSlider.print(brightnessPercent);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  strip.begin();
  strip.show();
  
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  Blinker.begin(AUTH, SSID, PASSWORD);
  Blinker.attachHeartbeat(heartbeat);
  
  RGB1.attach(rgbCallback);
  BrightnessSlider.attach(brightnessCallback);
  ModeBtn.attach(modeCallback);
  RelayBtn.attach(relayCallback);
  GradientBtn.attach(gradientCallback);
  BreathSpeedBtn.attach(breathSpeedCallback);
  FlowSpeedBtn.attach(flowSpeedCallback);
  GradientSpeedBtn.attach(gradientSpeedCallback);
  
  // 初始化按钮状态
  ModeBtn.icon("fas fa-lightbulb");
  ModeBtn.color("#4CAF50");
  ModeBtn.text("静态模式");
  ModeBtn.print();
  
  RelayBtn.icon("fas fa-toggle-off");
  RelayBtn.color("#9E9E9E");
  RelayBtn.text("继电器:关");
  RelayBtn.print();
  
  GradientBtn.icon("fas fa-ban");
  GradientBtn.color("#757575");
  GradientBtn.text("渐变色:关");
  GradientBtn.print();
  
  // 速度按钮初始化为慢速
  updateSpeedBtn(BreathSpeedBtn, 0, "呼吸");
  updateSpeedBtn(FlowSpeedBtn, 0, "流水");
  updateSpeedBtn(GradientSpeedBtn, 0, "渐变");
  
  RGB1.print(255, 0, 0);
  BrightnessSlider.print(50);
}

void loop() {
  Blinker.run();
  handleEffects();
  delay(20);
}
