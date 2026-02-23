#include "config.h"
#include "tasks.h"

void Task_UI(void *pvParameters) {
    float led_smooth = 0.0f;
    while (1) {
        uint8_t breath = beatsin8(20, 20, 255); 
        uint8_t fastFlash = beatsin8(120, 0, 255); 

          switch (currentMode) {
            case MODE_MEGAPHONE:     
                led_smooth = 0.8f * led_smooth + 0.2f * globalAudioEnergy;
                {
                    int brightness = (int)(log10f(led_smooth + 1.0f) * 60.0f);
                    brightness = constrain(brightness + 20, 20, 255); 
                    leds[0] = CHSV(HUE_RED, 255, brightness);
                }
                break;
            case MODE_WIFI_SPEAKER:  leds[0] = CHSV(HUE_BLUE, 255, breath); break;
            
            // ★ 修改这里：让录音状态有明确的视觉反馈
            case MODE_AI_ASSISTANT:  
                if (isAIRecording) {
                    leds[0] = CHSV(HUE_PURPLE, 255, 255); // 录音中：紫灯常亮
                } else if (isAIThinking) {
                    leds[0] = CHSV(HUE_YELLOW, 255, fastFlash); // 思考/网络请求中：黄灯快闪
                } else if (isAISpeaking) {
                    leds[0] = CHSV(HUE_BLUE, 255, breath); // AI 说话中：蓝灯呼吸
                } else {
                    leds[0] = CHSV(HUE_PURPLE, 255, breath); // 待机：紫灯呼吸
                }
                break;
                
            case MODE_WHITE_NOISE:   leds[0] = CHSV(HUE_GREEN, 255, breath); break;
            case MODE_SETUP_AP:      leds[0] = CHSV(HUE_YELLOW, 255, fastFlash); break;
        }
        
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}