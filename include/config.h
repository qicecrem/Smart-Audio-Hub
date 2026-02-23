#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <OneButton.h>

// ==================== 引脚与硬件定义 ====================
#define BTN_MODE_PIN       4   
#define LED_WS2812_PIN     48  
#define NUM_LEDS           1   

#define I2S_SCK_GPIO       16 // BCLK
#define I2S_WS_GPIO        17 // LRCLK
#define I2S_SD_IN_GPIO     18 // INMP441 SD
#define I2S_SD_OUT_GPIO    21 // MAX98357 DIN
#define I2S_PORT           I2S_NUM_0

// ==================== 系统模式枚举 ====================
enum SystemMode {
    MODE_MEGAPHONE,     // 0: 扩音器
    MODE_WIFI_SPEAKER,  // 1: Wi-Fi 音箱
    MODE_AI_ASSISTANT,  // 2: AI 助手
    MODE_WHITE_NOISE,   // 3: 白噪音/番茄钟
    MODE_SETUP_AP       // 4: 设置/配网模式
};

// ==================== 全局状态变量 (extern 声明) ====================
extern volatile SystemMode currentMode;
extern volatile bool modeChanged; 
extern volatile bool isWifiConnected; 
extern volatile float globalAudioEnergy; 

// 任务运行状态标志 (极度重要：确保 I2S 不冲突)
extern volatile bool isMegaphoneRunning;
extern volatile bool isWhiteNoiseRunning;
extern volatile bool isWebRadioRunning;
extern volatile bool isAIAssistantRunning; 
extern volatile bool isAIRecording;

extern volatile bool isAIThinking;  
extern volatile bool isAISpeaking;
// 全局硬件对象
extern CRGB leds[NUM_LEDS];
extern OneButton buttonMode;