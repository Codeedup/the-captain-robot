#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/ledc.h"

// PIN DEFINITIONS
#define MOTOR_L_IN1 5
#define MOTOR_L_IN2 6
#define MOTOR_R_IN3 7
#define MOTOR_R_IN4 8
#define SERVO_PIN 10 

// Global variable to remember the servo's current position (614 is dead center)
int current_servo_pos = 614;

// Data package, it must be the same on both sides
typedef struct __attribute__((packed))
{
    int left_joy_x;
    int left_joy_y;
    int right_joy_x;
    int right_joy_y;
    int left_button;
    int right_button;
} data_packet_t;

// ==========================================
// PWM SETUP FOR MOTORS & SERVO
// ==========================================
void init_pwm()
{
    // DC Motor Timer
    ledc_timer_config_t motor_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT, // 0 to 1023 speed
        .freq_hz = 20000, //changed from 1000 to 20000
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&motor_timer);

    // Servo Timer
    ledc_timer_config_t servo_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_13_BIT, // 0 to 8191 resolution
        .freq_hz = 50,                        // Servos require 50Hz
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&servo_timer);

    // DC Motor Channels
    ledc_channel_config_t ch_l1 = {.speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE, .gpio_num = MOTOR_L_IN1, .duty = 0, .hpoint = 0};
    ledc_channel_config_t ch_l2 = {.speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE, .gpio_num = MOTOR_L_IN2, .duty = 0, .hpoint = 0};
    ledc_channel_config_t ch_r1 = {.speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_2, .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE, .gpio_num = MOTOR_R_IN3, .duty = 0, .hpoint = 0};
    ledc_channel_config_t ch_r2 = {.speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_3, .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE, .gpio_num = MOTOR_R_IN4, .duty = 0, .hpoint = 0};

    // Servo Channel
    ledc_channel_config_t ch_srv = {.speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_4, .timer_sel = LEDC_TIMER_1, .intr_type = LEDC_INTR_DISABLE, .gpio_num = SERVO_PIN, .duty = current_servo_pos, .hpoint = 0};

    ledc_channel_config(&ch_l1);
    ledc_channel_config(&ch_l2);
    ledc_channel_config(&ch_r1);
    ledc_channel_config(&ch_r2);
    ledc_channel_config(&ch_srv);
}

void on_data_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len)
{
    data_packet_t packet;
    if (len != sizeof(data_packet_t))
        return; // Safety check
    memcpy(&packet, incomingData, sizeof(packet));

    // Print to monitor
    printf("LX:%d LY:%d | RX:%d RY:%d | L:%d R:%d\n",
           packet.left_joy_x, packet.left_joy_y,
           packet.right_joy_x, packet.right_joy_y,
           packet.left_button, packet.right_button);

    // ==========================================
    // DC MOTOR LOGIC
    // ==========================================
    // the speed is a total of 10bits, so the range has to be betwen 0 and 1024
    // --- LEFT MOTOR (Left Joystick Y) ---
    if (packet.left_joy_y == 100) 
    {
        // Full speed forward
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 800);
    }
    // else if (packet.left_joy_y == 75) 
    // {
        
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 500);
    // }
    // else if (packet.left_joy_y == 25) 
    // {
        
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 500);
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    // }
    else if (packet.left_joy_y == 0) 
    {
        // Full speed reverse
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    }
    else // LY is 50 (or any unexpected value)
    {
        // Stop
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    }

    // --- RIGHT MOTOR (Right Joystick Y) ---
    if (packet.right_joy_y == 100) 
    {
        // Full speed forward
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 800);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
    }
    // else if (packet.right_joy_y == 75) 
    // {
        
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 500);
    // }
    // else if (packet.right_joy_y == 25) 
    // {
        
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 500);
    //     ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
    // }
    else if (packet.right_joy_y == 0) 
    {
        // Full speed reverse
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 800);
    }
    else // RY is 50 (or any unexpected value)
    {
        // Stop
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3, 0);
    }

    // Apply the DC motor speeds
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_3);

    // ==========================================
    // SERVO BUTTON LOGIC
    // ==========================================
    
    // If Right Button is pressed, go immediately to the TOP
    if (packet.right_button == 1)
    {
        current_servo_pos = 819; //It was 819
    }
    // If Left Button is pressed, go immediately to the BOTTOM
    else if (packet.left_button == 1) 
    {
        current_servo_pos = 409; //decreased from 409
    }
    // If neither button is pressed (or both are 0), return to MIDDLE
    else 
    {
        current_servo_pos = 614; 
    }

    // Apply the new servo position immediately
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, current_servo_pos);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
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
    esp_now_register_recv_cb(on_data_recv);

    init_pwm();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
