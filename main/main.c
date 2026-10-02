#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"
#include "ble_service.h"

static const char *TAG = "MAIN";

// ==========================================
// PIN MAPPING CONFIGURATION
// ==========================================
#define HX711_DOUT_PIN    GPIO_NUM_4
#define HX711_SCK_PIN     GPIO_NUM_5
#define HALL_SENSOR_PIN   GPIO_NUM_6

// ==========================================
// PHYSICAL CALIBRATION CONSTANTS
// ==========================================
#define OFFSET            132148.0f         
#define CAL_FACTOR        1024.38f          
#define BOTTLE_WEIGHT     13.00f            
#define TABLET_WEIGHT     5.00f             

#define MEDIAN_WINDOW     7
#define STABILITY_STREAK  6

// Global tracking variables
static float previous_weight = 0.0f;
static int previous_tablet_count = 0;

static void hx711_init(void);
static long hx711_read(void);
static float get_weight_filtered(int sample_count);
static void get_timestamp(char *buf, size_t len);
static int compare_longs(const void *a, const void *b);
static bool is_cap_closed(void);

static int compare_longs(const void *a, const void *b) {
    long la = *(const long*)a;
    long lb = *(const long*)b;
    if (la < lb) return -1;
    if (la > lb) return 1;
    return 0;
}

static void hx711_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HX711_DOUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << HX711_SCK_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    
    gpio_set_level(HX711_SCK_PIN, 0);
}

static long hx711_read(void) {
    long count = 0;
    uint32_t timeout = 100000; 
    
    while (gpio_get_level(HX711_DOUT_PIN) && timeout > 0) {
        esp_rom_delay_us(1);
        timeout--;
    }

    if (timeout == 0) {
        return 0; 
    }

    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK_PIN, 1);
        esp_rom_delay_us(1);
        count = count << 1;
        gpio_set_level(HX711_SCK_PIN, 0);
        esp_rom_delay_us(1);
        if (gpio_get_level(HX711_DOUT_PIN)) {
            count++;
        }
    }

    gpio_set_level(HX711_SCK_PIN, 1);
    esp_rom_delay_us(1);
    gpio_set_level(HX711_SCK_PIN, 0);
    esp_rom_delay_us(1);

    if (count & 0x800000) {
        count |= ~0xFFFFFF;
    }

    return count;
}

static float get_weight_filtered(int sample_count) {
    long *samples = malloc(sizeof(long) * sample_count);
    if (!samples) {
        return previous_weight;
    }

    int valid_idx = 0;
    int consecutive_timeouts = 0;

    while (valid_idx < sample_count) {
        long val = hx711_read();
        if (val == 0) {
            consecutive_timeouts++;
            if (consecutive_timeouts > 15) {
                free(samples);
                return previous_weight;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        consecutive_timeouts = 0;
        samples[valid_idx] = val;
        valid_idx++;

        if (valid_idx % 10 == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    qsort(samples, sample_count, sizeof(long), compare_longs);

    int start_index = sample_count / 4;
    int end_index = (3 * sample_count) / 4;
    double sum = 0;
    int divisor = 0;

    for (int i = start_index; i < end_index; i++) {
        sum += (double)samples[i];
        divisor++;
    }

    free(samples);
    
    double avg_adc = sum / divisor;
    float calculated_weight = (float)((avg_adc - OFFSET) / CAL_FACTOR);
    
    if (calculated_weight < 0.10f) {
        calculated_weight = 0.00f;
    }
    
    return calculated_weight;
}

static void get_timestamp(char *buf, size_t len) {
    uint64_t total_seconds = esp_timer_get_time() / 1000000;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
    snprintf(buf, len, "%02u:%02u:%02u", (unsigned int)hours, (unsigned int)minutes, (unsigned int)seconds);
}

static bool is_cap_closed(void) {
    return gpio_get_level(HALL_SENSOR_PIN) == 0;
}

// ==========================================
// MAIN SCHEDULER APPLICATION
// ==========================================
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_config_t hall_conf = {
        .pin_bit_mask = (1ULL << HALL_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&hall_conf);

    hx711_init();
    ble_service_init();
    
    for (int i = 10; i > 0; i--) {
        printf("Starting scale... %d seconds\n", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    float weight_accumulator = 0.0f;
    int warm_cycles = 15;
    
    for (int i = 0; i < warm_cycles; i++) {
        weight_accumulator += get_weight_filtered(30);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    previous_weight = weight_accumulator / (float)warm_cycles; 
    float adjusted_w = previous_weight - BOTTLE_WEIGHT;
    if (adjusted_w < 0.10f) adjusted_w = 0.0f;

    float raw_tablets = adjusted_w / TABLET_WEIGHT;
    int base_tablets = (int)raw_tablets;
    float fractional_grams = (raw_tablets - base_tablets) * TABLET_WEIGHT;
    
    if (fractional_grams >= 2.0f) {
        base_tablets += 1;
    }
    previous_tablet_count = base_tablets;
    if (previous_tablet_count < 0) previous_tablet_count = 0;

    printf("\n====================================\n");
    printf("Current Scale Weight : %.2f g\n", previous_weight);
    printf("Current Tablet Count : %d\n", previous_tablet_count);
    printf("====================================\n\n");

    while (1) {
        printf("System entering sleep track mode. Monitoring window opens in 3 minutes...\n");
        vTaskDelay(pdMS_TO_TICKS(3 * 60 * 1000));

        char time_str[16];
        get_timestamp(time_str, sizeof(time_str));
        
        ESP_LOGI(TAG, "Broadcasting telemetry notifications. Executing alert cycle.");
        ble_service_notify("Medicine Time - Please take tablet");

        int64_t start_window = esp_timer_get_time();
        bool bottle_opened = false;

        // Monitor for cap removal during the 60 second tracking window
        while ((esp_timer_get_time() - start_window) < (60ULL * 1000000ULL)) {
            if (!is_cap_closed()) { 
                bottle_opened = true;
                printf("Bottle Opened\n");
                
                // FIXED: Explicitly tell Bluetooth app that cap status is now open
                ble_service_notify("Cap: OPENED"); 
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(40)); 
        }

        if (!bottle_opened) {
            get_timestamp(time_str, sizeof(time_str));
            
            printf("\n--- EVENT RECORDED ---\n");
            printf("Status:              TABLET MISSED\n");
            printf("Current Weight:      %.2fg\n", previous_weight);
            printf("Current Count:       %d\n", previous_tablet_count);
            printf("Timestamp:           %s\n", time_str);
            printf("----------------------\n\n");

            ble_service_notify("--- EVENT RECORDED ---");     vTaskDelay(pdMS_TO_TICKS(100));
            ble_service_notify("Status:              MISSED"); vTaskDelay(pdMS_TO_TICKS(100));
            
            char line_buf[64];
            snprintf(line_buf, sizeof(line_buf), "Weight: %.2fg", previous_weight);
            ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(100));
            
            snprintf(line_buf, sizeof(line_buf), "Count: %d", previous_tablet_count);
            ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(100));
            
            // FIXED: Send timestamp to BLE on a missed event window loop
            snprintf(line_buf, sizeof(line_buf), "Time: %s", time_str);
            ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(100));
            continue; 
        }

        while (!is_cap_closed()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        printf("Bottle Closed\n");
        
        // FIXED: Explicitly pass the closed cap status flag over to the BLE stack receiver
        ble_service_notify("Cap: CLOSED"); 
        vTaskDelay(pdMS_TO_TICKS(8 * 1000)); 

        float stable_sum = 0.0f;
        int successful_reads = 0;
        uint32_t stability_streak = 0;
        float last_check = get_weight_filtered(30); 

        while (stability_streak < STABILITY_STREAK) {
            vTaskDelay(pdMS_TO_TICKS(150));
            float current_check = get_weight_filtered(30); 
            
            if (fabsf(current_check - last_check) < 0.25f) {
                stability_streak++;
                stable_sum += current_check;
                successful_reads++;
            } else {
                stability_streak = 0;
                stable_sum = 0.0f;
                successful_reads = 0;
            }
            last_check = current_check;
        }

        float current_weight = stable_sum / (float)successful_reads;
        float current_adjusted_w = current_weight - BOTTLE_WEIGHT;
        if (current_adjusted_w < 0.10f) current_adjusted_w = 0.0f;
        
        float raw_tablets_run = current_adjusted_w / TABLET_WEIGHT;
        int base_tablets_run = (int)raw_tablets_run;
        float fractional_grams_run = (raw_tablets_run - base_tablets_run) * TABLET_WEIGHT;
        
        if (fractional_grams_run >= 2.0f) {
            base_tablets_run += 1;
        }
        int current_tablet_count = base_tablets_run;
        if (current_tablet_count < 0) current_tablet_count = 0;

        int removed = previous_tablet_count - current_tablet_count;
        float weight_delta = previous_weight - current_weight;

        const char* status_str;
        get_timestamp(time_str, sizeof(time_str));

        if ((weight_delta >= 2.0f) && (removed >= 1)) {
            status_str = "TABLET TAKEN";
        } else {
            status_str = "TABLET MISSED";
            current_weight = previous_weight;
            current_tablet_count = previous_tablet_count;
            removed = 0;
        }

        printf("\n--- EVENT RECORDED ---\n");
        printf("Status:              %s\n", status_str);
        printf("Current Weight:      %.2fg\n", current_weight);
        printf("Current Count:       %d\n", current_tablet_count);
        printf("Removed Tablets:     %d\n", removed);
        printf("Timestamp:           %s\n", time_str);
        printf("----------------------\n\n");

        ble_service_notify("--- EVENT RECORDED ---"); vTaskDelay(pdMS_TO_TICKS(100));
        
        char line_buf[64];
        snprintf(line_buf, sizeof(line_buf), "Status: %s", status_str);
        ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(5000));
        
        snprintf(line_buf, sizeof(line_buf), "Weight: %.2fg", current_weight);
        ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(5000));
        
        snprintf(line_buf, sizeof(line_buf), "Count: %d", current_tablet_count);
        ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(5000));
        
        snprintf(line_buf, sizeof(line_buf), "Removed: %d", removed);
        ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(5000));

        // FIXED: Streamed timestamp out down to the active app tracker after successful execution evaluation
        snprintf(line_buf, sizeof(line_buf), "Time: %s", time_str);
        ble_service_notify(line_buf); vTaskDelay(pdMS_TO_TICKS(5000));

        previous_weight = current_weight;
        previous_tablet_count = current_tablet_count;
    }
}
