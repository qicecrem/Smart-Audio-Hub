#include "config.h"
#include "tasks.h"
#include <WiFi.h>
#include <WiFiManager.h>

// ==================== 1. 定义全局变量 ====================
// 在 config.h 中声明为 extern，在这里分配真实的内存
volatile SystemMode currentMode = MODE_MEGAPHONE;
volatile bool modeChanged = true; 
volatile bool isWifiConnected = false; 
volatile float globalAudioEnergy = 0.0f; 

volatile bool isMegaphoneRunning = false; 
volatile bool isWhiteNoiseRunning = false;
volatile bool isWebRadioRunning = false;

volatile bool isAIAssistantRunning = false; 
volatile bool isAIRecording = false;
volatile bool isAIThinking = false;  // ★ 新增定义
volatile bool isAISpeaking = false;  // ★ 新增定义

CRGB leds[NUM_LEDS];
OneButton buttonMode(BTN_MODE_PIN, true, true);

// ==================== 2. 按键回调函数 ====================
void onButtonSingleClick() {
    if(currentMode == MODE_SETUP_AP) return; 
    int nextMode = (currentMode + 1) % 4;
    currentMode = (SystemMode)nextMode;
    modeChanged = true;
}

// ★ 新增：双击进入配网模式 (让出长按功能)
void onButtonDoubleClick() {
    currentMode = MODE_SETUP_AP;
    modeChanged = true;
}

// ★ 修改：按住按键开始录音 (仅在模式 3 生效)
void onButtonLongPressStart() {
    if (currentMode == MODE_AI_ASSISTANT) {
        isAIRecording = true;
        Serial.println("[UI] 按键按下，开始录音...");
    }
}

// ★ 新增：松开按键停止录音
void onButtonLongPressStop() {
    if (currentMode == MODE_AI_ASSISTANT) {
        isAIRecording = false;
        Serial.println("[UI] 按键松开，停止录音...");
    }
}
// ==================== 3. 核心状态机任务 ====================
void Task_AppManager(void *pvParameters) {
    // 【修复 1：利用 WiFiManager 强大的自带功能替代原先薄弱的重连代码】
    WiFiManager wm;
    // 设置配网热点为 3 分钟超时，防止设备永远卡在配网模式
    wm.setConfigPortalTimeout(180); 

    Serial.println("[系统] 正在尝试连接已保存的网络...");
    // autoConnect 会自动读取 NVS 的密码并尝试连接，连不上才会开启 AP
    // 这行代码执行完时，要么连上网了，要么配网超时了
    bool res = wm.autoConnect("SmartAudio_Setup");

     

    if(!res) {
        Serial.println("[警告] 无法连接网络或配网超时，将在单机模式下启动。");
        // 连不上没关系，我们仍然进入扩音器模式，只是没有 Wi-Fi 而已
    } else {
        Serial.println("[系统] 成功连接至 Wi-Fi!");
    }
    
    // 初始化完成后，确保系统状态为模式 1
    currentMode = MODE_MEGAPHONE;
    modeChanged = true;

    while (1) {
        if (modeChanged) {
            modeChanged = false;
            
              int timeoutCounter = 0;
            while(isMegaphoneRunning || isWhiteNoiseRunning || isWebRadioRunning || isAIAssistantRunning)  { 
                vTaskDelay(pdMS_TO_TICKS(10)); 
                timeoutCounter++;
                if(timeoutCounter > 300) { // 300 * 10ms = 3秒
                    Serial.println("!!! [严重警告] 音频任务卡死超时，防火墙强制放行 !!!");
                    // 强行把标志位拉低，防止系统彻底瘫痪
                    isMegaphoneRunning = false;
                    isWhiteNoiseRunning = false;
                    isWebRadioRunning = false;
                    break; 
                }
            } 

            Serial.println("\n=====================================");
            
            // 如果用户主动长按进入了配网模式
            if (currentMode == MODE_SETUP_AP) {
                Serial.println(">> [配网模式] 开启热点 SmartAudio_Setup ...");
                if (!wm.startConfigPortal("SmartAudio_Setup")) { 
                    Serial.println(">> 配网失败或超时，重启设备...");
                    delay(1000);
                    ESP.restart(); 
                }
                currentMode = MODE_MEGAPHONE; modeChanged = true; continue; 
            }

            switch (currentMode) {
                case MODE_MEGAPHONE:
                    Serial.println(">> [核心管理器] 启动 模式 1: 数字扩音器");
                    xTaskCreatePinnedToCore(Task_Megaphone, "T_Mega", 8192, NULL, 3, NULL, 1);
                    break;
                case MODE_WIFI_SPEAKER:
                    Serial.println(">> [核心管理器] 启动 模式 2: 网络流媒体电台");
                    if(WiFi.status() == WL_CONNECTED) {
                        // ★ 终极防卡顿优化 2：优先级提升至 5！
                        // 确保没有任何杂活（包括串口打印）能抢占音频解码的 CPU 时间
                        xTaskCreatePinnedToCore(Task_WebRadio, "T_Radio", 16384, NULL, 3, NULL, 1); 
                    } else {
                        Serial.println("   [警告] 未连接 Wi-Fi，电台无法启动。");
                    }
                    break;
                case MODE_AI_ASSISTANT:
                     Serial.println(">> [核心管理器] 启动 模式 3: AI 语音助手 (回放测试版)");
                    xTaskCreatePinnedToCore(Task_AIAssistant, "T_AI", 65536, NULL, 4, NULL, 1);
                    break;
                case MODE_WHITE_NOISE:
                    Serial.println(">> [核心管理器] 启动 模式 4: 海浪白噪音助眠");
                    xTaskCreatePinnedToCore(Task_WhiteNoise, "T_Noise", 4096, NULL, 3, NULL, 1);
                    break;
                default: break;
            }
            Serial.println("=====================================\n");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==================== 4. 系统入口 ====================
void setup() {
    Serial.begin(115200); delay(2000);

    // 硬件初始化
    FastLED.addLeds<WS2812, LED_WS2812_PIN, GRB>(leds, NUM_LEDS); FastLED.setBrightness(100);
    buttonMode.attachClick(onButtonSingleClick); 
    buttonMode.attachDoubleClick(onButtonDoubleClick); // 绑定双击
    buttonMode.attachLongPressStart(onButtonLongPressStart); // 绑定长按开始
    buttonMode.attachLongPressStop(onButtonLongPressStop);   // 绑定长按结束
    // 启动 FreeRTOS 任务
    xTaskCreatePinnedToCore(Task_UI, "Task_UI", 4096, NULL, 1, NULL, 0); 
    xTaskCreatePinnedToCore(Task_Network, "Task_NET", 4096, NULL, 1, NULL, 0); 
    xTaskCreatePinnedToCore(Task_AppManager, "Task_AppMGR", 8192, NULL, 2, NULL, 1); 
}

void loop() {
    buttonMode.tick();
    vTaskDelay(pdMS_TO_TICKS(10));
}