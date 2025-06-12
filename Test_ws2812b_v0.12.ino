 * //
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
 * //








#define BLINKER_WIFI
#include <Blinker.h>
#include <Adafruit_NeoPixel.h>

const char AUTH[] = "your_device_ker_here";                                      //点灯科技app密钥
const char SSID[] = "your_wifi_name";                                            //WiFi名字
const char PASSWORD[] = "your_wifi_password";                                    //wifi密码
const uint8_t RELAY_PIN = 0, LED_PIN = 2, LED_COUNT = 15, MAX_BRIGHTNESS = 150;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

BlinkerRGB RGB1("col-t3i");
BlinkerSlider BrightnessSlider("ran-8y7");
BlinkerButton ModeBtn("btn-gmq"), RelayBtn("btn-relay");
BlinkerButton GradientBtn("btn-834"), BreathSpeedBtn("btn-2da");
BlinkerButton FlowSpeedBtn("btn-mwk"), GradientSpeedBtn("btn-nw4");

uint8_t mode = 0, breathSpeed = 0, flowSpeed = 0, gradientSpeed = 0, brightness = 128;
bool relayState = false, gradientMode = false;
uint32_t color = 0xFF0000, secondaryColor = 0x0000FF;
int16_t breathValue = 128;
int8_t breathDirection = 1;
uint8_t flowPosition = 0, gradientStep = 0;

// 速度参数：慢、中、快对应的毫秒间隔
const uint16_t breathIntervals[] = {50, 30, 10};   // 呼吸效果更新间隔（毫秒）
const uint16_t flowIntervals[] = {200, 100, 50};    // 流水效果更新间隔
const uint16_t gradientIntervals[] = {200, 100, 50}; // 渐变效果在自动更新时的间隔

// 上一次更新时间
uint32_t lastBreathUpdate = 0;
uint32_t lastFlowUpdate = 0;
uint32_t lastGradientUpdate = 0;
uint32_t lastWiFiCheck = 0;
const uint32_t wifiCheckInterval = 10000; // 每10秒检查一次WiFi

void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on);
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
      uint8_t r = ((color>>16)&0xFF)*(1-ratio) + ((secondaryColor>>16)&0xFF)*ratio;
      uint8_t g = ((color>>8)&0xFF)*(1-ratio) + ((secondaryColor>>8)&0xFF)*ratio;
      uint8_t b = (color&0xFF)*(1-ratio) + (secondaryColor&0xFF)*ratio;
      strip.setPixelColor(i, r, g, b);
    }
    gradientStep = (gradientStep + 1) % LED_COUNT;  // 每次只移动1步，通过时间间隔控制速度
  } else {
    strip.fill(color);
  }
  strip.show();
}

void handleEffects() {
  uint32_t currentMillis = millis();
  
  // 呼吸效果
  if (mode == 1 && (currentMillis - lastBreathUpdate >= breathIntervals[breathSpeed])) {
    lastBreathUpdate = currentMillis;
    breathValue += breathDirection * (breathValue < 128 ? 1 : 2); // 在低亮度区域变化慢，高亮度区域变化快
    if (breathValue >= 255) {
      breathValue = 255;
      breathDirection = -1;
    } else if (breathValue <= 10) {
      breathValue = 10;
      breathDirection = 1;
    }
    // 呼吸效果需要实时更新亮度
    strip.setBrightness(breathValue);
    strip.fill(color);
    strip.show();
  }
  
  // 流水效果
  if (mode == 2 && (currentMillis - lastFlowUpdate >= flowIntervals[flowSpeed])) {
    lastFlowUpdate = currentMillis;
    strip.clear();
    for (int i = 0; i < 8; i++) {
      strip.setPixelColor((flowPosition + i) % LED_COUNT, color);
    }
    flowPosition = (flowPosition + 1) % LED_COUNT;
    strip.show();
  }
  
  // 渐变效果（在静态模式且开启渐变时，需要定时更新渐变位置）
  if (mode == 0 && gradientMode && (currentMillis - lastGradientUpdate >= gradientIntervals[gradientSpeed])) {
    lastGradientUpdate = currentMillis;
    // 更新渐变效果
    for (int i = 0; i < LED_COUNT; i++) {
      float ratio = (gradientStep + i * (gradientSpeed + 1)) % LED_COUNT / (float)LED_COUNT;
      uint8_t r = ((color>>16)&0xFF)*(1-ratio) + ((secondaryColor>>16)&0xFF)*ratio;
      uint8_t g = ((color>>8)&0xFF)*(1-ratio) + ((secondaryColor>>8)&0xFF)*ratio;
      uint8_t b = (color&0xFF)*(1-ratio) + (secondaryColor&0xFF)*ratio;
      strip.setPixelColor(i, r, g, b);
    }
    gradientStep = (gradientStep + 1) % LED_COUNT;
    strip.show();
  }
}

void syncSpeedButtons() {
  const char* icons[] = {"fas fa-tachometer-alt-slow", "fas fa-tachometer-alt-average", "fas fa-tachometer-alt-fast"};
  const char* colors[] = {"#FFEB3B", "#2196F3", "#F44336"};
  const char* breathTexts[] = {"呼吸:慢速", "呼吸:中速", "呼吸:快速"};
  const char* flowTexts[] = {"流水:慢速", "流水:中速", "流水:快速"};
  const char* gradientTexts[] = {"渐变:慢速", "渐变:中速", "渐变:快速"};
  
  BreathSpeedBtn.icon(icons[breathSpeed]);
  BreathSpeedBtn.color(colors[breathSpeed]);
  BreathSpeedBtn.text(breathTexts[breathSpeed]);
  BreathSpeedBtn.print();

  FlowSpeedBtn.icon(icons[flowSpeed]);
  FlowSpeedBtn.color(colors[flowSpeed]);
  FlowSpeedBtn.text(flowTexts[flowSpeed]);
  FlowSpeedBtn.print();

  GradientSpeedBtn.icon(icons[gradientSpeed]);
  GradientSpeedBtn.color(colors[gradientSpeed]);
  GradientSpeedBtn.text(gradientTexts[gradientSpeed]);
  GradientSpeedBtn.print();
}

void heartbeat() {
  ModeBtn.print();
  RelayBtn.print();
  GradientBtn.print();
  syncSpeedButtons();
  
  uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
  RGB1.print(r, g, b);
  BrightnessSlider.print(map(brightness, 5, MAX_BRIGHTNESS, 0, 100));
}

void rgbCallback(uint8_t r, uint8_t g, uint8_t b, uint8_t) { 
  color = strip.Color(r, g, b); 
  // 当颜色改变时，立即更新LED
  updateLEDs();
}

void brightnessCallback(int32_t value) { 
  brightness = map(value, 0, 100, 5, MAX_BRIGHTNESS); 
  updateLEDs(); 
}

void modeCallback(const String&) { 
  mode = (mode + 1) % 3; 
  const char* icons[] = {"fas fa-lightbulb", "fas fa-wind", "fas fa-water"};
  const char* colors[] = {"#4CAF50","#FF9800","#2196F3"};
  const char* texts[] = {"静态模式","呼吸模式","流水模式"};
  ModeBtn.icon(icons[mode]); ModeBtn.color(colors[mode]); ModeBtn.text(texts[mode]); ModeBtn.print();
  // 切换模式后立即更新LED显示
  if (mode == 0) {
    // 静态模式：直接更新
    updateLEDs();
  } else if (mode == 1) {
    // 呼吸模式：重置呼吸状态
    breathValue = 128;
    breathDirection = 1;
  } else if (mode == 2) {
    // 流水模式：重置位置
    flowPosition = 0;
  }
}

void relayCallback(const String&) { setRelay(!relayState); }

void gradientCallback(const String&) { 
  gradientMode = !gradientMode; 
  GradientBtn.text(gradientMode ? "渐变色:开" : "渐变色:关");
  GradientBtn.icon(gradientMode ? "fas fa-fill-drip" : "fas fa-ban");
  GradientBtn.color(gradientMode ? "#9C27B0" : "#757575");
  GradientBtn.print();
  updateLEDs();
}

void breathSpeedCallback(const String&) { breathSpeed = (breathSpeed + 1) % 3; syncSpeedButtons(); }
void flowSpeedCallback(const String&) { flowSpeed = (flowSpeed + 1) % 3; syncSpeedButtons(); }
void gradientSpeedCallback(const String&) { gradientSpeed = (gradientSpeed + 1) % 3; syncSpeedButtons(); }

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.begin(SSID, PASSWORD);
    uint8_t attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
      delay(500);
      attempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi");
    } else {
      Serial.println("Failed to reconnect to WiFi");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  strip.begin();
  strip.show();
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());
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
  
  // 初始化状态
  ModeBtn.icon("fas fa-lightbulb"); ModeBtn.color("#4CAF50"); ModeBtn.text("静态模式"); ModeBtn.print();
  RelayBtn.icon("fas fa-toggle-off"); RelayBtn.color("#9E9E9E"); RelayBtn.text("继电器:关"); RelayBtn.print();
  GradientBtn.icon("fas fa-ban"); GradientBtn.color("#757575"); GradientBtn.text("渐变色:关"); GradientBtn.print();
  
  // 初始化速度按钮
  breathSpeed = 0; flowSpeed = 0; gradientSpeed = 0;
  syncSpeedButtons();
  
  RGB1.print(255, 0, 0);
  BrightnessSlider.print(50);
}

void loop() {
  Blinker.run();
  handleEffects();
  
  uint32_t currentMillis = millis();
  if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = currentMillis;
    checkWiFi();
  }
  
  // 非阻塞延迟，确保循环快速执行
  delay(2);
}
