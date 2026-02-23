#pragma once

// Core 0 任务
void Task_UI(void *pvParameters);
void Task_Network(void *pvParameters);

// Core 1 任务
void Task_AppManager(void *pvParameters);
void Task_Megaphone(void *pvParameters);
void Task_WhiteNoise(void *pvParameters);
void Task_WebRadio(void *pvParameters); 
void Task_AIAssistant(void *pvParameters);