#include "config.h"
#include "tasks.h"
#include <driver/i2s.h>
#include <math.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h> 

#include "AudioFileSourcePROGMEM.h" 
#include "AudioFileSourceICYStream.h" 
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#define DASHSCOPE_API_KEY "sk-acd1922a545c4486a5eb55c6f6fb992c"


// ==================== ★ STT 终极严谨发送版 (修复 body invalid) ====================
String bailian_STT(uint8_t* audio_data_in_psram, uint32_t len) {
    if (len == 0) return "";
    
    WiFiClientSecure client;
    client.setInsecure(); 
    client.setTimeout(20000); 

    Serial.println("[STT] 连接阿里云...");
    if (!client.connect("dashscope.aliyuncs.com", 443)) {
        Serial.println("[STT] 连接失败");
        return "";
    }

    // HTTP 1.0 协议头 (使用 HTTP/1.0 强制短连接，避免 Keep-Alive 问题)
    String header = "POST /api/v1/services/audio/asr/recognition HTTP/1.0\r\n";
    header += "Host: dashscope.aliyuncs.com\r\n";
    header += "Authorization: Bearer " DASHSCOPE_API_KEY "\r\n";
    header += "Content-Type: application/octet-stream\r\n";
    header += "X-DashScope-DSP: pcm\r\n";
    header += "X-DashScope-SampleRate: 16000\r\n";
    header += "X-DashScope-Format: pcm\r\n";
    header += "Content-Length: " + String(len) + "\r\n";
    header += "Connection: close\r\n\r\n"; // ★ 双换行符，极其重要！
    
    // 一次性发送 Header，确保不分包
    client.print(header);
    client.flush(); 

    Serial.printf("[STT] 开始严谨上传 %d 字节...\n", len);
    
    const uint32_t SRAM_CHUNK_SIZE = 1024; 
    uint8_t sram_buffer[SRAM_CHUNK_SIZE]; 
    
    uint32_t total_sent = 0;
    
    while (total_sent < len) {
        if (!client.connected()) { Serial.println("[STT] 断连！"); break; }

        // 1. 计算本次搬运大小
        uint32_t chunk_len = (len - total_sent > SRAM_CHUNK_SIZE) ? SRAM_CHUNK_SIZE : (len - total_sent);
        
        // 2. 从 PSRAM 搬到 SRAM
        memcpy(sram_buffer, &audio_data_in_psram[total_sent], chunk_len);
        
        // 3. ★ 严谨循环发送：确保 chunk_len 全部发完
        size_t written_this_chunk = 0;
        while (written_this_chunk < chunk_len) {
            size_t w = client.write(sram_buffer + written_this_chunk, chunk_len - written_this_chunk);
            if (w == 0) {
                // 如果写入 0 字节，可能是网络拥塞，稍微等一下
                vTaskDelay(1);
                if (!client.connected()) break;
            }
            written_this_chunk += w;
        }
        
        total_sent += chunk_len;
        
        // 4. 打印进度
        if (total_sent % 10240 == 0) {
            Serial.printf("  -> %d%%\n", (total_sent * 100) / len);
            vTaskDelay(1); 
        }
    }
    
    client.flush();
    Serial.println("[STT] 上传完毕，等待响应...");

    // 读取响应
    String response = "";
    bool jsonStarted = false;
    uint32_t waitStart = millis();
    
    // ★ 增加延时等待服务器处理完上一帧数据
    while ((client.connected() || client.available()) && millis() - waitStart < 15000) {
        if (client.available()) {
            char c = client.read();
            if (c == '{') jsonStarted = true;
            if (jsonStarted) response += c;
        } else {
            vTaskDelay(10);
        }
    }
    client.stop();

    if (response.length() > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, response);
        if (error) {
            Serial.println("[STT] JSON 解析失败！原始响应:");
            Serial.println(response);
            return "";
        }

        if (doc.containsKey("output") && doc["output"].containsKey("sentence")) {
            String result = doc["output"]["sentence"][0]["text"].as<String>();
            Serial.println("[STT] 识别成功: " + result);
            return result;
        } else if (doc.containsKey("message")) {
             Serial.println("[STT] 阿里云报错: " + doc["message"].as<String>());
             // 如果报错，把 code 也打印出来方便查错
             if(doc.containsKey("code")) Serial.println("Code: " + doc["code"].as<String>());
        }
    } else {
        Serial.println("[STT] 空响应");
    }
    return "";
}


// 2. 大模型对话 (LLM) - 使用 Qwen-Turbo (通义千问)
String bailian_LLM(String user_text) {
    if (user_text.length() == 0) return "";
    
    HTTPClient http;
    http.begin("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"); // 兼容 OpenAI 格式
    
    http.addHeader("Authorization", "Bearer " DASHSCOPE_API_KEY);
    http.addHeader("Content-Type", "application/json");

    // 构建 JSON 请求体
    DynamicJsonDocument reqDoc(1024);
    reqDoc["model"] = "qwen-turbo";
    JsonArray messages = reqDoc.createNestedArray("messages");
    JsonObject systemMsg = messages.createNestedObject();
    systemMsg["role"] = "system";
    systemMsg["content"] = "你是一个智能音箱助手，请用简短、幽默的一两句话回答用户，不要长篇大论。";
    JsonObject userMsg = messages.createNestedObject();
    userMsg["role"] = "user";
    userMsg["content"] = user_text;

    String requestBody;
    serializeJson(reqDoc, requestBody);

    int httpCode = http.POST(requestBody);
    String reply = "";
    
    if (httpCode == 200) {
        String response = http.getString();
        DynamicJsonDocument resDoc(2048);
        deserializeJson(resDoc, response);
        reply = resDoc["choices"][0]["message"]["content"].as<String>();
        Serial.println("[LLM] AI回答: " + reply);
    } else {
        Serial.printf("[LLM] 请求失败: %d\n", httpCode);
    }
    http.end();
    return reply;
}

// 3. 文字转语音 (TTS) - 下载 MP3 到内存
// 返回下载到的音频数据指针，长度写入 out_len
uint8_t* bailian_TTS(String text, uint32_t* out_len) {
    if (text.length() == 0) return NULL;
    
    HTTPClient http;
    http.begin("https://dashscope.aliyuncs.com/api/v1/services/audio/tts/synthesis");
    
    http.addHeader("Authorization", "Bearer " DASHSCOPE_API_KEY);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument reqDoc(1024);
    reqDoc["model"] = "sambert-zhichu-v1"; // 知初：亲切女声
    JsonObject input = reqDoc.createNestedObject("input");
    input["text"] = text;
    JsonObject params = reqDoc.createNestedObject("parameters");
    params["format"] = "mp3";
    params["sample_rate"] = 16000; // 降低采样率减小体积

    String requestBody;
    serializeJson(reqDoc, requestBody);

    int httpCode = http.POST(requestBody);
    uint8_t* mp3_buff = NULL;
    *out_len = 0;

    if (httpCode == 200) {
        int size = http.getSize(); // 获取内容长度
        if (size > 0) {
            // 分配 PSRAM 内存来存 MP3
            mp3_buff = (uint8_t*)ps_malloc(size);
            if (mp3_buff) {
                WiFiClient* stream = http.getStreamPtr();
                int readBytes = stream->readBytes(mp3_buff, size);
                *out_len = readBytes;
                Serial.printf("[TTS] 语音合成下载完成: %d bytes\n", readBytes);
            } else {
                Serial.println("[TTS] PSRAM 内存不足，无法下载语音！");
            }
        }
    } else {
        Serial.printf("[TTS] 请求失败: %d\n", httpCode);
        Serial.println(http.getString());
    }
    http.end();
    return mp3_buff;
}




// ==================== 模式 1：扩音器 DSP ====================
void Task_Megaphone(void *pvParameters) {
    Serial.println("[音频系统] 扩音器 I2S 驱动加载中...");
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
        .sample_rate = 44100, .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 4, .dma_buf_len = 128,
        .use_apll = false, .tx_desc_auto_clear = true, .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_GPIO, .ws_io_num = I2S_WS_GPIO,
        .data_out_num = I2S_SD_OUT_GPIO, .data_in_num = I2S_SD_IN_GPIO
    };
    
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);

    int32_t sample_buffer[256]; int32_t output_buffer[256];
    float dc_mean = 0.0f; float prev_pcm = 0.0f; const float lpf_alpha = 0.8f;
    isMegaphoneRunning = true;

    while (currentMode == MODE_MEGAPHONE) {
        size_t bytes_read = 0;
        if (i2s_read(I2S_PORT, sample_buffer, sizeof(sample_buffer), &bytes_read, portMAX_DELAY) == ESP_OK && bytes_read > 0) {
            int samples = bytes_read / sizeof(int32_t);
            float energy_acc = 0.0f;
            for (int i = 0; i < samples; i++) {
                int32_t raw = sample_buffer[i] >> 8;
                dc_mean = dc_mean * 0.995f + raw * 0.005f; float pcm = raw - dc_mean;
                pcm = lpf_alpha * pcm + (1.0f - lpf_alpha) * prev_pcm; prev_pcm = pcm;
                float abs_pcm = fabsf(pcm);
                if (abs_pcm < 50000.0f) { pcm = 0.0f; abs_pcm = 0.0f; }
                if (abs_pcm > 100000.0f) { float excess = abs_pcm - 100000.0f; pcm = copysignf(100000.0f + excess / 15.0f, pcm); }
                pcm *= 0.65f;
                if (pcm > 8388607.0f) pcm = 8388607.0f; if (pcm < -8388608.0f) pcm = -8388608.0f;
                output_buffer[i] = ((int32_t)pcm) << 8; energy_acc += abs_pcm;
            }
            size_t bytes_written;
            i2s_write(I2S_PORT, output_buffer, bytes_read, &bytes_written, portMAX_DELAY);
            globalAudioEnergy = energy_acc / samples; 
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    Serial.println("[音频系统] 扩音器已停止，正在卸载 I2S...");
    i2s_driver_uninstall(I2S_PORT); 
    globalAudioEnergy = 0.0f; isMegaphoneRunning = false;
    vTaskDelete(NULL); 
}

// ==================== 模式 4：白噪音海浪声 ====================
void Task_WhiteNoise(void *pvParameters) {
    // ...(白噪音代码完全不变)...
    isWhiteNoiseRunning = true;
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), .sample_rate = 44100, .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 4, .dma_buf_len = 256, .use_apll = false, .tx_desc_auto_clear = true, .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK_GPIO, .ws_io_num = I2S_WS_GPIO, .data_out_num = I2S_SD_OUT_GPIO, .data_in_num = I2S_PIN_NO_CHANGE };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL); i2s_set_pin(I2S_PORT, &pin_config);

    int32_t output_buffer[128]; float brown_noise = 0.0f;
    while (currentMode == MODE_WHITE_NOISE) {
        for (int i = 0; i < 128; i++) {
            float white = ((float)random(2000000) / 1000000.0f) - 1.0f; brown_noise = (brown_noise + (0.02f * white)) / 1.02f; 
            float pcm = brown_noise * 1500000.0f; output_buffer[i] = ((int32_t)pcm) << 8;
        }
        size_t bytes_written; i2s_write(I2S_PORT, output_buffer, sizeof(output_buffer), &bytes_written, portMAX_DELAY); vTaskDelay(pdMS_TO_TICKS(1)); 
    }
    i2s_driver_uninstall(I2S_PORT); isWhiteNoiseRunning = false; vTaskDelete(NULL);
}

// ==================== ★ 模式 2：网络流媒体电台 (逻辑修复无卡顿版) ====================
void Task_WebRadio(void *pvParameters) {
    isWebRadioRunning = true;
    const char* streamURL = "http://lhttp.qingting.fm/live/5022107/64k.mp3";
    
    // 初始化指针为 NULL，防止 delete 野指针
    AudioFileSourceICYStream *radioFile = nullptr;
    AudioFileSourceBuffer *radioBuff = nullptr;
    AudioGeneratorMP3 *radioMP3 = nullptr;
    AudioOutputI2S *radioOut = nullptr;

    Serial.println("[WebRadio] 启动任务...");

    // 1. 安全初始化资源
    try {
        radioFile = new AudioFileSourceICYStream(streamURL);
        // 如果有 PSRAM，建议加大到 64KB 或 128KB
        radioBuff = new AudioFileSourceBuffer(radioFile, 64 * 1024); 
        radioMP3 = new AudioGeneratorMP3();
        radioOut = new AudioOutputI2S(0, 0, 16, 256);
        
        radioOut->SetPinout(I2S_SCK_GPIO, I2S_WS_GPIO, I2S_SD_OUT_GPIO);
        radioOut->SetGain(0.1f);
        radioOut->SetOutputModeMono(true);
    } catch (...) {
        Serial.println("[WebRadio] 内存申请失败！");
        goto cleanup; // 跳转到资源释放区
    }

    // 2. 主循环：增加弱网重试逻辑
    while (currentMode == MODE_WIFI_SPEAKER) {
        if (!radioMP3->isRunning()) {
            Serial.println("[WebRadio] 尝试连接服务器...");
            if (!radioMP3->begin(radioBuff, radioOut)) {
                Serial.println("[WebRadio] 连接失败，5秒后重试...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        // 3. 解码循环
        if (radioMP3->isRunning()) {
            if (!radioMP3->loop()) {
                // 这里通常是由于网络断开导致缓冲区跑空
                Serial.println("[WebRadio] 播放中断，检查网络...");
                radioMP3->stop(); 
                // 弱网下不要立刻 break，给网络一点恢复时间
                vTaskDelay(pdMS_TO_TICKS(1000)); 
            }
        }

        // 适当增加延时，确保后台 WiFi 栈有足够时间处理数据
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }

cleanup:
    Serial.println("[WebRadio] 正在释放资源...");
    if (radioMP3) { radioMP3->stop(); delete radioMP3; }
    if (radioBuff) { radioBuff->close(); delete radioBuff; }
    if (radioFile) { radioFile->close(); delete radioFile; }
    if (radioOut)  { delete radioOut; }

    isWebRadioRunning = false;
    Serial.println("[WebRadio] 任务已安全退出");
    vTaskDelete(NULL);
}

// ==================== ★ 模式 3：AI 助手 (Phase 5.1 录音回放测试) ====================
// ==================== ★ 模式 3：AI 语音助手 (Phase 5.2 终极完整版) ====================
void Task_AIAssistant(void *pvParameters) {
    isAIAssistantRunning = true;
    Serial.println("[AI Assistant] 任务启动，请按住按键说话！");

    const uint32_t MAX_REC_SIZE = 160 * 1024; 
    uint8_t *rec_buffer = (uint8_t *)ps_malloc(MAX_REC_SIZE);
    
    if (!rec_buffer) {
        Serial.println("[AI] 致命错误：PSRAM 分配失败！");
        isAIAssistantRunning = false; vTaskDelete(NULL); return;
    }

    uint32_t rec_len = 0;

    while (currentMode == MODE_AI_ASSISTANT) {
        
        // --- 1. 录音阶段 ---
        if (isAIRecording) {
            Serial.println("[AI] 麦克风开启 (16kHz)...");
            i2s_config_t i2s_in = {
                .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                .sample_rate = 16000, .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
                .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 256,
                .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
            };
            i2s_pin_config_t pin_in = { .bck_io_num = I2S_SCK_GPIO, .ws_io_num = I2S_WS_GPIO, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD_IN_GPIO };
            i2s_driver_install(I2S_PORT, &i2s_in, 0, NULL); i2s_set_pin(I2S_PORT, &pin_in);

            rec_len = 0; int32_t sample_buf[64]; 
            Serial.println("[AI] 🔴 正在录音...");
            
            while (isAIRecording && rec_len < MAX_REC_SIZE - 256) {
                size_t bytes_read = 0;
                i2s_read(I2S_PORT, sample_buf, sizeof(sample_buf), &bytes_read, portMAX_DELAY);
                for (int i = 0; i < bytes_read / 4; i++) {
                    int16_t pcm16 = (int16_t)(sample_buf[i] >> 16); 
                    rec_buffer[rec_len++] = pcm16 & 0xFF;
                    rec_buffer[rec_len++] = (pcm16 >> 8) & 0xFF;
                }
            }
            Serial.printf("[AI] ⏹️ 录音结束，共 %d 字节\n", rec_len);
            i2s_driver_uninstall(I2S_PORT); 

            // --- 2. 思考与交互阶段 ---
            if (rec_len > 1000 && currentMode == MODE_AI_ASSISTANT) {
                isAIThinking = true; // 黄灯闪烁
                
                // (A) 发送 STT
                String user_text = bailian_STT(rec_buffer, rec_len);
                
                if (user_text.length() > 0) {
                    // (B) 发送 LLM
                    String ai_reply = bailian_LLM(user_text);
                    
                    if (ai_reply.length() > 0) {
                        // (C) 获取 TTS 音频 (下载到 tts_buffer)
                        uint32_t tts_len = 0;
                        uint8_t* tts_buffer = bailian_TTS(ai_reply, &tts_len);
                        
                        isAIThinking = false; 

                        // (D) 播放回答
                        if (tts_buffer && tts_len > 0) {
                            isAISpeaking = true; // 蓝灯呼吸
                            Serial.println("[AI] 🔊 开始播报回答...");

                            // 使用 PROGMEM 流 (虽然名字叫 PROGMEM，但其实支持读取 RAM 指针)
                            AudioFileSourcePROGMEM *fileSource = new AudioFileSourcePROGMEM(tts_buffer, tts_len);
                            AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();
                            AudioOutputI2S *out = new AudioOutputI2S(0, 0, 16, 256);
                            out->SetPinout(I2S_SCK_GPIO, I2S_WS_GPIO, I2S_SD_OUT_GPIO);
                            out->SetGain(0.2f); // 适中音量
                            out->SetOutputModeMono(true);

                            mp3->begin(fileSource, out);
                            while (mp3->isRunning() && currentMode == MODE_AI_ASSISTANT) {
                                if (!mp3->loop()) mp3->stop();
                                vTaskDelay(1);
                            }
                            
                            delete mp3; delete out; delete fileSource;
                            free(tts_buffer); // ★ 务必释放下载的 TTS 内存
                            isAISpeaking = false;
                        }
                    }
                }
                isAIThinking = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }

    Serial.println("[AI] 退出 AI 模式...");
    free(rec_buffer);
    isAIAssistantRunning = false;
    vTaskDelete(NULL);
}