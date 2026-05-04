#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

uint8_t receiver_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define LEFT_BTN_PIN 13
#define RIGHT_BTN_PIN 12

#define MY_ROBOT_ID 2

typedef struct __attribute__((packed))
{
    uint8_t robot_id;
    int left_joy_x;
    int left_joy_y;
    int right_joy_x;
    int right_joy_y;
    int left_button;
    int right_button;
} data_packet_t;

int map_joystick(int raw_value)
{
    // Map raw ADC (0-4095) to a base percentage (0-100)
    int mapped = (raw_value * 100) / 4095;

    if (mapped >= 40 && mapped <= 60)
    {
        return 50;
    }
    else if (mapped <= 5)
    {
        return 100;
    }
    else if (mapped >= 95)
    {
        return 0;
    }
    else if (mapped < 40)
    {              
        return 75; 
    }
    else
    {
        return 25;
    }
}

void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    esp_now_init();
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = 1; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {.bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12};
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config);
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config);
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config);
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &config);

    gpio_set_direction(LEFT_BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LEFT_BTN_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(RIGHT_BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RIGHT_BTN_PIN, GPIO_PULLUP_ONLY);

    data_packet_t packet;
    int raw_lx, raw_ly, raw_rx, raw_ry;

    packet.robot_id = MY_ROBOT_ID;

    while (1)
    {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &raw_lx);
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &raw_ly);
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &raw_rx);
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &raw_ry);

        packet.left_joy_x = map_joystick(raw_lx);
        packet.left_joy_y = map_joystick(raw_ly);
        packet.right_joy_x = map_joystick(raw_rx);
        packet.right_joy_y = map_joystick(raw_ry);
        packet.left_button = !gpio_get_level(RIGHT_BTN_PIN);
        packet.right_button = !gpio_get_level(LEFT_BTN_PIN);

        printf("ID:%d | LX:%d LY:%d | RX:%d RY:%d | LBtn:%d RBtn:%d\n",
               packet.robot_id, packet.left_joy_x, packet.left_joy_y,
               packet.right_joy_x, packet.right_joy_y,
               packet.left_button, packet.right_button);

        esp_now_send(receiver_mac, (uint8_t *)&packet, sizeof(packet));
        // Temporarily put this inside your while(1) loop to see the resting values
        // printf("RAW LX:%d LY:%d | RX:%d RY:%d\n", raw_lx, raw_ly, raw_rx, raw_ry);
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz is much more stable than 200Hz (5ms)
    }
}
