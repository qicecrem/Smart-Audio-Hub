# 🎵 ESP32-S3 AI 智能音频终端 (Smart Audio Hub)

> **基于 ESP32-S3 + FreeRTOS 的多功能智能音频开发框架**  
> 集成数字扩音、网络电台、白噪音助眠、以及 **ChatGPT/DeepSeek 级别的 AI 语音对话** 功能。

![Build Status](https://img.shields.io/badge/PlatformIO-Build%20Passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--S3-orange)

## ✨ 项目简介

本项目旨在利用 **ESP32-S3** 强大的双核处理能力和 AI 指令集，打造一个低成本、高可玩性的开源智能音箱。
区别于传统的蓝牙音箱，它无需依赖手机，可独立连接 Wi-Fi 播放全球电台，甚至直接与云端大模型进行语音对话。

核心亮点：
*   🚀 **FreeRTOS 多任务架构**：音频解码、网络传输、UI 交互互不干扰，极度丝滑。
*   🎙️ **工业级防啸叫算法**：内置 DSP 数字信号处理，实现专业级扩音器功能。
*   📻 **零卡顿网络电台**：自研 ICY 流媒体缓冲算法，抗弱网抖动。
*   🤖 **AI 语音对话**：集成了阿里云百炼 (DashScope) 全链路语音交互 (STT -> LLM -> TTS)。

---

## 🛠️ 硬件准备

| 组件 | 型号/参数 | 连接引脚 (ESP32-S3) |
| :--- | :--- | :--- |
| **主控** | ESP32-S3 DevKitC-1 (N16R8) | - |
| **麦克风** | INMP441 (I2S 全向麦) | SCK(16), WS(17), SD(18) |
| **功放** | MAX98357A (I2S DAC) | SCK(16), WS(17), DIN(21) |
| **按键** | 轻触按键 (低电平触发) | GPIO 4 |
| **灯珠** | WS2812 RGB (板载) | GPIO 48 |

> ⚠️ **注意**：本项目强依赖 **PSRAM** (8MB)，请确保您的开发板型号支持 (如 N8R8 或 N16R8)。

---

## 📦 功能模式 (5-in-1)

通过 **短按** 按键循环切换以下模式，**WS2812 状态灯** 指示当前模式：

### 1. 🔴 数字扩音器 (Megaphone Mode)
*   **灯光**：红灯呼吸 (随音量律动)
*   **功能**：采集麦克风声音，经过 **降噪、去直流、防啸叫 (移频/陷波)** 处理后，实时从喇叭放出。
*   **用途**：教学扩音、导游喊话。

### 2. 🔵 网络电台 (Web Radio Mode)
*   **灯光**：蓝灯呼吸
*   **功能**：连接 Wi-Fi，播放互联网 MP3/AAC 直播流。
*   **特性**：内置 32KB 抖动缓冲池，支持断线重连，音质纯净。

### 3. 🟣 AI 语音助手 (AI Chat Mode)
*   **灯光**：紫灯 (待机) -> 黄灯快闪 (思考中) -> 蓝灯 (回答中)
*   **交互**：
    1.  **按住按键**：开始录音 (支持 16kHz 高清语音)。
    2.  **松开按键**：自动上传至云端。
    3.  **AI 回答**：播放大模型生成的语音回复。
*   **技术栈**：阿里云 Paraformer (语音转文字) + Qwen-Turbo (大模型) + Sambert (文字转语音)。

### 4. 🟢 白噪音助眠 (White Noise Mode)
*   **灯光**：绿灯呼吸
*   **功能**：利用 ESP32 算力实时生成 **布朗噪音 (Brownian Noise)**，模拟海浪/瀑布声。
*   **用途**：助眠、专注力训练、掩盖环境噪音。

### 5. 🟡 配网模式 (AP Config Mode)
*   **灯光**：黄灯快闪
*   **触发**：**双击** 按键进入。
*   **功能**：开启热点 `SmartAudio_Setup`，手机连接后自动弹出网页配置 Wi-Fi 密码。

---

## 🚀 快速开始

### 1. 环境搭建
*   安装 [VSCode](https://code.visualstudio.com/)。
*   安装插件 **PlatformIO IDE**。

