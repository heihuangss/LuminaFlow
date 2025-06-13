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
 * 版本：0.13
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
#include <math.h>

const char AUTH[] = "your_device_ker_here";                                      //点灯科技app密钥
const char SSID[] = "your_wifi_name";                                            //WiFi名字
const char PASSWORD[] = "your_wifi_password";                                    //wifi密码

const uint8_t RELAY_PIN = 0;
const uint8_t LED_PIN = 2;
const uint8_t LED_COUNT = 15;
const uint8_t MAX_BRIGHTNESS = 150;

struct DeviceState {
  uint8_t mode = 0;
  uint8_t breathSpeed = 1;
  uint8_t flowSpeed = 1;
  uint8_t gradientSpeed = 1;
  uint8_t brightness = 128;
  bool relayState = false;
  bool gradientMode = false;
  bool colorControlEnabled = true;

  uint32_t primaryColor = 0xFF0000;
  uint32_t savedPrimaryColor = 0xFF0000;

  int breathLevel = 128;
  int breathDirection = 1;
  uint8_t flowPosition = 0;
  uint8_t gradientStep = 0;
};

DeviceState deviceState;

BlinkerRGB RGB1("col-t3i");
BlinkerSlider BrightnessSlider("ran-8y7");
BlinkerButton ModeBtn("btn-gmq");
BlinkerButton RelayBtn("btn-relay");
BlinkerButton GradientBtn("btn-834");
BlinkerButton BreathSpeedBtn("btn-2da");
BlinkerButton FlowSpeedBtn("btn-mwk");
BlinkerButton GradientSpeedBtn("btn-nw4");

const uint16_t breathIntervals[] = {60, 30, 15};
const uint16_t flowIntervals[] = {250, 100, 50};
const uint16_t gradientIntervals[] = {200, 100, 50};
const uint32_t wifiCheckInterval = 30000;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

namespace ColorScience {
  // 快速立方根近似
  float fastCbrt(float x) {
    if (x == 0.0f) return 0.0f;
    union { float f; uint32_t i; } conv = {x};
    conv.i = (conv.i / 3 + 0x2a5137a3);
    float y = conv.f;
    y = 0.333333f * (2.0f * y + x / (y * y)); // 一次牛顿迭代提高精度
    return y;
  }

  // Gamma校正 - 将sRGB转换为线性RGB
  void gammaExpand(uint8_t channel, float &linear) {
    float normalized = channel / 255.0f;
    linear = (normalized <= 0.04045f) ? normalized / 12.92f : powf((normalized + 0.055f)/1.055f, 2.4f);
  }

  // RGB转XYZ (D65白点)
  void rgbToXyz(uint8_t r, uint8_t g, uint8_t b, float &x, float &y, float &z) {
    float rLin, gLin, bLin;
    gammaExpand(r, rLin);
    gammaExpand(g, gLin);
    gammaExpand(b, bLin);
    
    x = 0.4124564f * rLin + 0.3575761f * gLin + 0.1804375f * bLin;
    y = 0.2126729f * rLin + 0.7151522f * gLin + 0.0721750f * bLin;
    z = 0.0193339f * rLin + 0.1191920f * gLin + 0.9503041f * bLin;
  }
  
  // XYZ转OKLab (感知均匀色彩空间)
  void xyzToOklab(float x, float y, float z, float &l, float &a, float &b) {
    float l_ = 0.818933f * x + 0.361866f * y - 0.128097f * z;
    float m_ = 0.032984f * x + 0.929318f * y + 0.036158f * z;
    float s_ = 0.048200f * x + 0.264366f * y + 0.633546f * z;
    
    l_ = fastCbrt(l_);
    m_ = fastCbrt(m_);
    s_ = fastCbrt(s_);
    
    l = 0.210454f * l_ + 0.793618f * m_ - 0.004072f * s_;
    a = 1.977998f * l_ - 2.428592f * m_ + 0.450594f * s_;
    b = 0.025904f * l_ + 0.782772f * m_ - 0.808676f * s_;
  }
  
  // OKLab转XYZ
  void oklabToXyz(float l, float a, float bb, float &x, float &y, float &z) {
    float l_ = l + 0.3963377774f * a + 0.2158037573f * bb;
    float m_ = l - 0.1055613458f * a - 0.0638541728f * bb;
    float s_ = l - 0.0894841775f * a - 1.2914855480f * bb;
    
    l_ = l_ * l_ * l_;
    m_ = m_ * m_ * m_;
    s_ = s_ * s_ * s_;
    
    x = 1.2270138511f * l_ - 0.5577999807f * m_ + 0.2812561490f * s_;
    y = -0.0405801783f * l_ + 1.1122568696f * m_ - 0.0716766787f * s_;
    z = -0.0763812845f * l_ - 0.4214819784f * m_ + 1.5861632204f * s_;
  }
  
  // XYZ转sRGB
  void xyzToRgb(float x, float y, float z, uint8_t &r, uint8_t &g, uint8_t &b) {
    float rLin = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
    float gLin = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
    float bLin = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;
    
    auto gammaCompress = [](float linear) -> float {
      if (linear <= 0.0031308f) return 12.92f * linear;
      return 1.055f * powf(linear, 1.0f/2.4f) - 0.055f;
    };
    
    rLin = constrain(gammaCompress(rLin), 0.0f, 1.0f);
    gLin = constrain(gammaCompress(gLin), 0.0f, 1.0f);
    bLin = constrain(gammaCompress(bLin), 0.0f, 1.0f);
    
    r = (uint8_t)(rLin * 255);
    g = (uint8_t)(gLin * 255);
    b = (uint8_t)(bLin * 255);
  }
  
  // 完整RGB到OKLCH转换
  void rgbToOklch(uint32_t color, float &l, float &c, float &h) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    float x, y, z;
    rgbToXyz(r, g, b, x, y, z);
    
    float labL, labA, labB;
    xyzToOklab(x, y, z, labL, labA, labB);
    
    // 转换为柱坐标(OKLCH)
    c = sqrtf(labA * labA + labB * labB);
    h = atan2f(labB, labA) * 180.0f / M_PI;
    if (h < 0) h += 360.0f;
    l = labL;
  }
  
  // OKLCH转RGB
  uint32_t oklchToRgb(float l, float c, float h) {
    float a = c * cosf(h * M_PI / 180.0f);
    float b = c * sinf(h * M_PI / 180.0f);
    
    float x, y, z;
    oklabToXyz(l, a, b, x, y, z);
    
    uint8_t r, g, bVal;
    xyzToRgb(x, y, z, r, g, bVal);
    
    return strip.Color(r, g, bVal);
  }
}

namespace GradientEngine {
  // 感知均匀的渐变算法
  uint32_t perceptualGradient(uint32_t color1, uint32_t color2, float ratio) {
    // 获取颜色1的参数
    float l1, c1, h1;
    ColorScience::rgbToOklch(color1, l1, c1, h1);
    
    // 获取颜色2的参数
    float l2, c2, h2;
    ColorScience::rgbToOklch(color2, l2, c2, h2);
    
    // 处理色相最短路径
    float deltaHue = h2 - h1;
    if (fabsf(deltaHue) > 180.0f) {
      if (deltaHue > 0) h2 -= 360.0f;
      else h1 -= 360.0f;
    }
    
    // 插值 - 使用缓动函数使过渡更自然
    float t = 0.5f - 0.5f * cosf(ratio * M_PI); // 余弦缓动
    float l = l1 + t * (l2 - l1);
    float c = c1 + t * (c2 - c1);
    float h = h1 + t * (h2 - h1);
    
    // 处理色相环绕
    if (h > 360.0f) h -= 360.0f;
    if (h < 0.0f) h += 360.0f;
    
    // 转换为RGB
    return ColorScience::oklchToRgb(l, c, h);
  }
}

namespace LEDControl {
  // 为LED生成渐变颜色的核心函数
  uint32_t generateLedColor(uint8_t position) {
    if (!deviceState.gradientMode) {
      return deviceState.primaryColor;
    }
    
    // 创建和谐的5色色盘（基于OKLCH）
    float baseL, baseC, baseH;
    ColorScience::rgbToOklch(deviceState.primaryColor, baseL, baseC, baseH);
    
    uint32_t colors[5] = {
      ColorScience::oklchToRgb(baseL, baseC, baseH), // 主色
      ColorScience::oklchToRgb(baseL * 1.1f, baseC * 0.9f, fmodf(baseH + 72.0f, 360.0f)),
      ColorScience::oklchToRgb(baseL * 0.9f, baseC * 1.1f, fmodf(baseH + 144.0f, 360.0f)),
      ColorScience::oklchToRgb(baseL * 1.2f, baseC * 0.8f, fmodf(baseH + 216.0f, 360.0f)),
      ColorScience::oklchToRgb(baseL * 0.8f, baseC * 1.2f, fmodf(baseH + 288.0f, 360.0f))
    };
    
    // 动态时间偏移
    float timeOffset = sinf(millis() * 0.0005f * (1.0f + deviceState.gradientSpeed * 0.5f)) * 0.1f;
    
    // 计算LED的位置值（0.0-1.0）
    float ledPos = (position + deviceState.gradientStep) % LED_COUNT / (float)LED_COUNT;
    ledPos = fmodf(ledPos + timeOffset, 1.0f);
    
    // 在色盘中循环
    float palettePos = ledPos * 5.0f;
    int index1 = (int)palettePos % 5;
    int index2 = (index1 + 1) % 5;
    float blend = palettePos - index1;
    
    return GradientEngine::perceptualGradient(colors[index1], colors[index2], blend);
  }

  void updateLEDs() {
    uint8_t actualBrightness = min(deviceState.brightness, MAX_BRIGHTNESS);
    strip.setBrightness(actualBrightness);
    
    switch(deviceState.mode) {
      case 0: // 静态模式
        for (int i = 0; i < LED_COUNT; i++) {
          uint32_t color = generateLedColor(i);
          strip.setPixelColor(i, color);
        }
        break;
        
      case 1: // 呼吸模式
        {
          strip.setBrightness(deviceState.breathLevel);
          if (deviceState.gradientMode) {
            for (int i = 0; i < LED_COUNT; i++) {
              uint32_t color = generateLedColor(i);
              strip.setPixelColor(i, color);
            }
          } else {
            strip.fill(deviceState.primaryColor, 0, LED_COUNT);
          }
        }
        break;
        
      case 2: // 流水模式
        {
          strip.clear();
          for (int i = 0; i < 4; i++) {
            uint16_t pos = (deviceState.flowPosition + i) % LED_COUNT;
            uint32_t color = deviceState.gradientMode ? 
                             generateLedColor(i) : 
                             deviceState.primaryColor;
            strip.setPixelColor(pos, color);
          }
        }
        break;
    }
    strip.show();
  }
}

namespace RelayControl {
  void setRelay(bool newState) {
    digitalWrite(RELAY_PIN, newState);
    deviceState.relayState = newState;
    RelayBtn.icon(newState ? "fas fa-toggle-on" : "fas fa-toggle-off");
    RelayBtn.color(newState ? "#FF5252" : "#9E9E9E");
    RelayBtn.text(newState ? "继电器:开" : "继电器:关");
    RelayBtn.print();
  }
}

namespace Effects {
  void handleEffects() {
    static uint32_t lastBreath = 0;
    static uint32_t lastFlow = 0;
    static uint32_t lastGradient = 0;
    uint32_t currentMillis = millis();
    
    if (deviceState.mode == 1 && currentMillis - lastBreath >= breathIntervals[deviceState.breathSpeed]) {
      lastBreath = currentMillis;
      deviceState.breathLevel += deviceState.breathDirection * (deviceState.breathLevel < 128 ? 1 : 2);
      
      if (deviceState.breathLevel >= 255) {
        deviceState.breathLevel = 255;
        deviceState.breathDirection = -1;
      } else if (deviceState.breathLevel <= 10) {
        deviceState.breathLevel = 10;
        deviceState.breathDirection = 1;
      }
      LEDControl::updateLEDs();
    }
    
    if (deviceState.mode == 2 && currentMillis - lastFlow >= flowIntervals[deviceState.flowSpeed]) {
      lastFlow = currentMillis;
      deviceState.flowPosition = (deviceState.flowPosition + 1) % LED_COUNT;
      LEDControl::updateLEDs();
    }
    
    if (deviceState.mode == 0 && deviceState.gradientMode && currentMillis - lastGradient >= gradientIntervals[deviceState.gradientSpeed]) {
      lastGradient = currentMillis;
      deviceState.gradientStep = (deviceState.gradientStep + 1) % LED_COUNT;
      LEDControl::updateLEDs();
    }
  }
}

namespace UIControl {
  void syncSpeedButtons() {
    const char* breathTexts[] = {"呼吸:慢速", "呼吸:中速", "呼吸:快速"};
    const char* flowTexts[] = {"流水:慢速", "流水:中速", "流水:快速"};
    const char* gradientTexts[] = {"渐变:慢速", "渐变:中速", "渐变:快速"};
    
    const char* icons[] = {"fas fa-tachometer-alt-slow", "fas fa-tachometer-alt-average", "fas fa-tachometer-alt-fast"};
    const char* colors[] = {"#FFEB3B", "#2196F3", "#F44336"};
    
    BreathSpeedBtn.icon(icons[deviceState.breathSpeed]);
    BreathSpeedBtn.color(colors[deviceState.breathSpeed]);
    BreathSpeedBtn.text(breathTexts[deviceState.breathSpeed]);
    BreathSpeedBtn.print();
    
    FlowSpeedBtn.icon(icons[deviceState.flowSpeed]);
    FlowSpeedBtn.color(colors[deviceState.flowSpeed]);
    FlowSpeedBtn.text(flowTexts[deviceState.flowSpeed]);
    FlowSpeedBtn.print();
    
    GradientSpeedBtn.icon(icons[deviceState.gradientSpeed]);
    GradientSpeedBtn.color(colors[deviceState.gradientSpeed]);
    GradientSpeedBtn.text(gradientTexts[deviceState.gradientSpeed]);
    GradientSpeedBtn.print();
  }

  void heartbeat() {
    ModeBtn.print();
    RelayBtn.print();
    GradientBtn.print();
    syncSpeedButtons();
    
    if (deviceState.colorControlEnabled) {
      uint8_t r = (deviceState.primaryColor >> 16) & 0xFF;
      uint8_t g = (deviceState.primaryColor >> 8) & 0xFF;
      uint8_t b = deviceState.primaryColor & 0xFF;
      RGB1.print(r, g, b, 255);
    }
    BrightnessSlider.print(map(deviceState.brightness, 0, MAX_BRIGHTNESS, 0, 100));
  }
}

namespace Network {
  void checkWiFi() {
    static uint32_t lastWiFiCheck = 0;
    uint32_t currentMillis = millis();
    
    if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
      lastWiFiCheck = currentMillis;
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(SSID, PASSWORD);
      }
    }
  }
}

namespace Callbacks {
  void rgbCallback(uint8_t r, uint8_t g, uint8_t b, uint8_t) { 
    if (deviceState.colorControlEnabled) {
      deviceState.primaryColor = strip.Color(r, g, b);
      deviceState.savedPrimaryColor = deviceState.primaryColor;
      LEDControl::updateLEDs();
    }
  }

  void brightnessCallback(int32_t value) { 
    deviceState.brightness = map(value, 0, 100, 0, MAX_BRIGHTNESS);
    LEDControl::updateLEDs(); 
  }

  void modeCallback(const String&) { 
    deviceState.mode = (deviceState.mode + 1) % 3;
    
    const char* icons[] = {"fas fa-lightbulb", "fas fa-wind", "fas fa-water"};
    const char* texts[] = {"静态模式", "呼吸模式", "流水模式"};
    const char* btnColors[] = {"#4CAF50", "#FF9800", "#2196F3"};
    
    ModeBtn.icon(icons[deviceState.mode]);
    ModeBtn.color(btnColors[deviceState.mode]);
    ModeBtn.text(texts[deviceState.mode]);
    ModeBtn.print();
    
    if (deviceState.mode == 1) {
      deviceState.breathLevel = 128;
      deviceState.breathDirection = 1;
    } else if (deviceState.mode == 2) {
      deviceState.flowPosition = 0;
    }
    LEDControl::updateLEDs();
  }

  void relayCallback(const String&) { 
    RelayControl::setRelay(!deviceState.relayState); 
  }

  void gradientCallback(const String&) { 
    deviceState.gradientMode = !deviceState.gradientMode;
    
    if (deviceState.gradientMode) {
      if (deviceState.colorControlEnabled) {
        deviceState.savedPrimaryColor = deviceState.primaryColor;
      }
      deviceState.colorControlEnabled = false;
      
      GradientBtn.text("渐变色:开");
      GradientBtn.icon("fas fa-fill-drip");
      GradientBtn.color("#9C27B0");
    } else {
      deviceState.primaryColor = deviceState.savedPrimaryColor;
      deviceState.colorControlEnabled = true;
      
      GradientBtn.text("渐变色:关");
      GradientBtn.icon("fas fa-ban");
      GradientBtn.color("#757575");
    }
    
    GradientBtn.print();
    LEDControl::updateLEDs();
  }

  void breathSpeedCallback(const String&) { 
    deviceState.breathSpeed = (deviceState.breathSpeed + 1) % 3; 
    UIControl::syncSpeedButtons();
  }

  void flowSpeedCallback(const String&) { 
    deviceState.flowSpeed = (deviceState.flowSpeed + 1) % 3; 
    UIControl::syncSpeedButtons();
  }

  void gradientSpeedCallback(const String&) { 
    deviceState.gradientSpeed = (deviceState.gradientSpeed + 1) % 3; 
    UIControl::syncSpeedButtons();
    LEDControl::updateLEDs();
  }
}

namespace Initialization {
  void setupHardware() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    randomSeed(analogRead(0));
    strip.begin();
    strip.setBrightness(min(deviceState.brightness, MAX_BRIGHTNESS));
    strip.fill(deviceState.primaryColor, 0, LED_COUNT);
    strip.show();
    deviceState.savedPrimaryColor = deviceState.primaryColor;
  }

  void setupWiFi() {
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    Serial.print("已连接, IP: ");
    Serial.println(WiFi.localIP());
  }

  void setupBlinker() {
    Blinker.begin(AUTH, SSID, PASSWORD);
    Blinker.attachHeartbeat(UIControl::heartbeat);
    
    RGB1.attach(Callbacks::rgbCallback);
    BrightnessSlider.attach(Callbacks::brightnessCallback);
    ModeBtn.attach(Callbacks::modeCallback);
    RelayBtn.attach(Callbacks::relayCallback);
    GradientBtn.attach(Callbacks::gradientCallback);
    BreathSpeedBtn.attach(Callbacks::breathSpeedCallback);
    FlowSpeedBtn.attach(Callbacks::flowSpeedCallback);
    GradientSpeedBtn.attach(Callbacks::gradientSpeedCallback);
    
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
    
    UIControl::syncSpeedButtons();
    
    Serial.println("系统初始化完成 - 专业级渐变系统已启动");
  }
}

void setup() {
  Initialization::setupHardware();
  Initialization::setupWiFi();
  Initialization::setupBlinker();
}

void loop() {
  Blinker.run();
  Effects::handleEffects();
  Network::checkWiFi();
  delay(5);
}
