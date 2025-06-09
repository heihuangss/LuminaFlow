#define BLINKER_WIFI
#include <Blinker.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

// ========== 用户配置区 ==========
char auth[] = "your_blinker_auth_key"; // Blinker设备密钥
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";
const int relayPin = 0;              // GPIO0控制继电器
const int ledPin = 2;                // GPIO2控制WS2812B
#define LED_COUNT 60                 // 灯珠数量
#define EEPROM_SIZE 64               // EEPROM大小
#define DATA_ADDR 0                  // 数据存储起始地址
// ===============================

// 断电记忆数据结构
struct PersistentData {
  uint8_t mode;           // 当前模式
  uint32_t color;         // 当前颜色
  uint8_t brightness;     // 目标亮度（8位存储）
  bool gradientEnabled;   // 渐变色开关
};

// 全局变量
int ledMode = 0;                    // 0:静态 1:呼吸灯 2:流水灯
uint32_t currentColor = 0xFF0000;   // 默认红色
unsigned long prevGradientMillis = 0; // 渐变效果专用定时器
unsigned long prevBreathMillis = 0;   // 呼吸效果专用定时器
unsigned long prevFlowMillis = 0;    // 流水效果专用定时器
float currentBrightness = 128.0;     // 当前亮度
float targetBrightness = 128.0;      // 目标亮度
float breathBrightness = 128.0;     // 呼吸模式亮度
int breathDirection = 1;            // 呼吸灯方向
int flowPosition = 0;               // 流水灯位置
bool relayState = false;            // 继电器状态（不存储）
bool gradientEnabled = false;       // 渐变色模式开关
int hueValue = 0;                   // 色相值 (0-360)
int hueDirection = 1;               // 色相变化方向
const float LERP_FACTOR = 0.05;     // 插值系数
bool brightnessControlEnabled = true; // 亮度控制是否启用
bool ledsEnabled = true;            // LED是否启用

// 呼吸模式档位控制变量
int breathSpeedLevel = 1;           // 0:慢速, 1:中速, 2:快速
float breathSpeeds[3] = {0.3, 0.5, 1.0}; // 各档位对应的步长值

// 硬件对象
Adafruit_NeoPixel strip(LED_COUNT, ledPin, NEO_GRB + NEO_KHZ800);

// Blinker组件
BlinkerRGB RGB1((const char*)"col-t3i");          // 颜色选择器
BlinkerSlider BrightnessSlider((const char*)"ran-8y7"); // 亮度滑动条
BlinkerButton ModeBtn((const char*)"btn-gmq");    // 模式切换按钮
BlinkerButton RelayBtn((const char*)"btn-relay"); // 继电器控制按钮
BlinkerButton GradientBtn((const char*)"btn-834"); // 渐变色功能键
BlinkerButton BreathSpeedBtn((const char*)"btn-2da"); // 呼吸模式档位开关

// 保存配置到EEPROM
void saveConfig() {
  PersistentData data;
  data.mode = ledMode;
  data.color = currentColor;
  data.brightness = (uint8_t)targetBrightness;
  data.gradientEnabled = gradientEnabled;
  
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(DATA_ADDR, data);
  EEPROM.commit();
  EEPROM.end();
}

// 从EEPROM加载配置
void loadConfig() {
  PersistentData data;
  
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(DATA_ADDR, data);
  EEPROM.end();
  
  // 检查是否有有效数据
  if (data.mode <= 2 && data.brightness >= 0 && data.brightness <= 255) {
    ledMode = data.mode;
    currentColor = data.color;
    targetBrightness = data.brightness;
    gradientEnabled = data.gradientEnabled;
  }
}

// 呼吸模式档位切换回调
void breathSpeed_callback(const String &state) {
  // 循环切换三个档位
  breathSpeedLevel = (breathSpeedLevel + 1) % 3;
  
  // 更新按钮文本显示当前档位
  switch(breathSpeedLevel) {
    case 0: 
      BreathSpeedBtn.text("呼吸:慢速");
      BreathSpeedBtn.color("#0000FF"); // 蓝色
      break;
    case 1: 
      BreathSpeedBtn.text("呼吸:中速");
      BreathSpeedBtn.color("#00FF00"); // 绿色
      break;
    case 2: 
      BreathSpeedBtn.text("呼吸:快速");
      BreathSpeedBtn.color("#FF0000"); // 红色
      break;
  }
  BreathSpeedBtn.print();
}

// 亮度控制回调
void brightness_callback(int32_t value) {
  if (!brightnessControlEnabled) return;
  
  // 滑动条值0-100 → 亮度5-255
  targetBrightness = map(value, 0, 100, 5, 255); 
  
  // 当亮度设为0时关闭LED
  ledsEnabled = (value > 0);
  
  // 立即保存配置
  saveConfig();
}

// 颜色选择回调
void rgb1_callback(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  currentColor = strip.Color(r, g, b);
  
  // 关闭渐变色模式
  gradientEnabled = false;
  GradientBtn.text("渐变色:关");
  GradientBtn.color("#FF0000");
  GradientBtn.print();
  
  // 静态模式下立即更新颜色
  if (ledMode == 0) {
    updateAllLEDs();
  }
  
  // 立即保存配置
  saveConfig();
}

// 模式切换回调
void mode_callback(const String &state) {
  ledMode = (ledMode + 1) % 3;  // 循环切换模式
  
  // 更新按钮文本
  switch(ledMode) {
    case 0: 
      ModeBtn.text("静态模式");
      brightnessControlEnabled = true;
      BrightnessSlider.color("#FFFFFF"); 
      BrightnessSlider.print();
      break;
    case 1: 
      ModeBtn.text("呼吸模式");
      breathBrightness = currentBrightness;
      brightnessControlEnabled = false;
      BrightnessSlider.color("#CCCCCC");
      BrightnessSlider.print();
      break;
    case 2: 
      ModeBtn.text("流水模式");
      flowPosition = 0;
      brightnessControlEnabled = true;
      BrightnessSlider.color("#FFFFFF");
      BrightnessSlider.print();
      break;
  }
  ModeBtn.print();
  
  // 立即保存配置
  saveConfig();
}

// 继电器控制回调
void relay_callback(const String &state) {
  relayState = !relayState;
  digitalWrite(relayPin, relayState ? HIGH : LOW);
  
  RelayBtn.text(relayState ? "断开继电器" : "开启继电器");
  RelayBtn.print();
}

// 渐变色功能回调
void gradient_callback(const String &state) {
  gradientEnabled = !gradientEnabled;
  
  if (gradientEnabled) {
    GradientBtn.text("渐变色:开");
    GradientBtn.color("#00FF00");
    hueValue = 0;
  } else {
    GradientBtn.text("渐变色:关");
    GradientBtn.color("#FF0000");
  }
  
  GradientBtn.print();
  
  // 立即保存配置
  saveConfig();
}

// 线性插值函数
float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// HSV转RGB函数
uint32_t hsvToRgb(uint16_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  h %= 360;
  
  uint8_t sector = h / 60;
  uint8_t offset = h % 60;
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * offset) / 60))) >> 8;
  uint8_t t_val = (v * (255 - ((s * (60 - offset)) / 60))) >> 8;
  
  switch (sector) {
    case 0: r = v; g = t_val; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t_val; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t_val; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  
  return strip.Color(r, g, b);
}

// 更新色相值
void updateGradient() {
  if (millis() - prevGradientMillis > 30) {
    prevGradientMillis = millis();
    hueValue = (hueValue + 2) % 360;
    currentColor = hsvToRgb(hueValue, 255, 255);
  }
}

// 更新亮度
void updateBrightness() {
  currentBrightness = lerp(currentBrightness, targetBrightness, LERP_FACTOR);
  
  if (abs(currentBrightness - targetBrightness) < 0.5) {
    currentBrightness = targetBrightness;
  }
  
  strip.setBrightness((uint8_t)currentBrightness);
}

// 呼吸灯效果
void breathEffect() {
  float breathStep = breathSpeeds[breathSpeedLevel];
  
  if(millis() - prevBreathMillis > 20) {
    prevBreathMillis = millis();
    breathBrightness += breathDirection * breathStep;
    
    if(breathBrightness > 255) {
      breathBrightness = 255;
      breathDirection = -1;
    } else if(breathBrightness < 5) {
      breathBrightness = 5;
      breathDirection = 1;
    }
    
    currentBrightness = lerp(currentBrightness, breathBrightness, 0.1f);
    strip.setBrightness((uint8_t)currentBrightness);
    
    if (ledsEnabled) {
      updateAllLEDs();
    } else {
      strip.clear();
      strip.show();
    }
  }
}

// 流水灯效果
void flowEffect() {
  #if defined(ESP8266) && LED_COUNT > 256
    const int flowLength = 16;
  #else
    const int flowLength = min(24, LED_COUNT/2);
  #endif
  
  if(millis() - prevFlowMillis > 100) {
    prevFlowMillis = millis();
    updateBrightness();
    
    if (ledsEnabled) {
      strip.clear();
      for(int i=0; i<flowLength; i++) {
        int pos = (flowPosition + i) % LED_COUNT;
        if (gradientEnabled) {
          uint16_t hue = (hueValue + i * 24) % 360;
          strip.setPixelColor(pos, hsvToRgb(hue, 255, 255));
        } else {
          strip.setPixelColor(pos, currentColor);
        }
      }
      strip.show();
      flowPosition = (flowPosition + 1) % LED_COUNT;
    } else {
      strip.clear();
      strip.show();
    }
  }
}

// 更新所有灯珠颜色
void updateAllLEDs() {
  strip.fill(currentColor);
  strip.show();
}

void setup() {
  Serial.begin(115200);
  
  // 加载灯具配置
  loadConfig();
  
  // 继电器初始化
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  
  // WS2812B初始化
  strip.begin();
  strip.setBrightness(currentBrightness);
  strip.fill(currentColor);
  strip.show();
  
  // WiFi连接
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  // Blinker配置
  Blinker.begin(auth, ssid, password);
  RGB1.attach(rgb1_callback);
  BrightnessSlider.attach(brightness_callback);
  ModeBtn.attach(mode_callback);
  RelayBtn.attach(relay_callback);
  GradientBtn.attach(gradient_callback);
  BreathSpeedBtn.attach(breathSpeed_callback);
  
  // 初始化按钮状态
  switch(ledMode) {
    case 0: ModeBtn.text("静态模式"); break;
    case 1: ModeBtn.text("呼吸模式"); break;
    case 2: ModeBtn.text("流水模式"); break;
  }
  ModeBtn.print();
  
  RelayBtn.text("开启继电器");
  RelayBtn.print();
  
  if (gradientEnabled) {
    GradientBtn.text("渐变色:开");
    GradientBtn.color("#00FF00");
  } else {
    GradientBtn.text("渐变色:关");
    GradientBtn.color("#FF0000");
  }
  GradientBtn.print();
  
  int sliderValue = map(targetBrightness, 5, 255, 0, 100);
  BrightnessSlider.print(sliderValue);
  
  ledsEnabled = (sliderValue > 0);
  
  if (ledMode == 1) {
    brightnessControlEnabled = false;
    BrightnessSlider.color("#CCCCCC");
    BrightnessSlider.print();
  }
  
  // 初始化呼吸模式档位按钮
  switch(breathSpeedLevel) {
    case 0: 
      BreathSpeedBtn.text("呼吸:慢速");
      BreathSpeedBtn.color("#0000FF");
      break;
    case 1: 
      BreathSpeedBtn.text("呼吸:中速");
      BreathSpeedBtn.color("#00FF00");
      break;
    case 2: 
      BreathSpeedBtn.text("呼吸:快速");
      BreathSpeedBtn.color("#FF0000");
      break;
  }
  BreathSpeedBtn.print();
}

void loop() {
  Blinker.run();
  
  if (gradientEnabled && ledMode != 1) {
    updateGradient();
  }
  
  switch(ledMode) {
    case 0:
      updateBrightness();
      if (gradientEnabled) updateGradient();
      if (ledsEnabled) updateAllLEDs(); else {strip.clear(); strip.show();}
      break;
    case 1:
      if (gradientEnabled) updateGradient();
      breathEffect();
      break;
    case 2:
      flowEffect();
      break;
  }
  
  delay(5);
  
  #if defined(ESP8266) && LED_COUNT > 100
    static unsigned long lastGC = 0;
    if (millis() - lastGC > 30000) {
      lastGC = millis();
      ESP.wdtDisable();
      ESP.resetFreeContStack();
      ESP.wdtEnable(WDTO_4S);
    }
  #endif
}