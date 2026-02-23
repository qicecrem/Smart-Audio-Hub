#include "config.h"
#include "tasks.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.aliyun.com", 8 * 3600, 60000); 

void Task_Network(void *pvParameters) {
    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!isWifiConnected) {
                isWifiConnected = true;
                timeClient.begin(); 
            }
            timeClient.update();
        } else {
            isWifiConnected = false;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}