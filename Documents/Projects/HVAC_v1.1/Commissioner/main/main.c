#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_openthread.h"
#include "esp_task_wdt.h"

// --- OpenThread Includes ---
#include "openthread/instance.h"
#include "openthread/thread.h"        // Needed for otGetVersionString and Role
#include "openthread/commissioner.h"  // Needed for otCommissionerStart/Stop
#include "openthread/error.h"         // Needed for otError definitions
#include "esp_openthread_lock.h"      // IMPORTANT: Needed for locking mechanism

#include "thread_init.h"
#include "commissioner.h" // CRITICAL: This header must include your wrapper prototype
#include "uart_rx.h"

static const char *TAG = "MAIN";

// --- Network State Monitor ---
static void on_thread_state_changed(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    // Only process OpenThread events
    if (event_base != OPENTHREAD_EVENT) {
        return;
    }

    otInstance *instance = esp_openthread_get_instance();

    if (event_id == OPENTHREAD_EVENT_ATTACHED) {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        
        // AUTO-START LOGIC: If we become Leader, force Commissioner ON
        if (role == OT_DEVICE_ROLE_LEADER) {
            if (esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
                
                otCommissionerState state = otCommissionerGetState(instance);
                
                // Only try to start if NOT already active
                if (state != OT_COMMISSIONER_STATE_ACTIVE) {
                    
                    // --- CRITICAL FIX START ---
                    // OLD: otCommissionerStart(instance, NULL, NULL, NULL); 
                    // NEW: Call the wrapper that registers the callbacks!
                    commissioner_start(); 
                    // --- CRITICAL FIX END ---

                } else {
                    ESP_LOGI(TAG, "Commissioner already ACTIVE (Leader)");
                }
                
                esp_openthread_lock_release();
            } else {
                ESP_LOGE(TAG, "Could not acquire lock to start Commissioner");
            }
        }
    } 
    else if (event_id == OPENTHREAD_EVENT_DETACHED) {
        ESP_LOGW(TAG, "Network Detached! Attempting recovery...");
    }
}

void app_main(void)
{
    // 1. Initialize Watchdog (Optional, enable if production requires strict timeouts)
    // esp_task_wdt_init(10, true);

    // 2. NVS with Recovery
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corruption detected. Erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Critical NVS Failure. Restarting...");
        esp_restart();
    }

    // 3. Event Loop
    if (esp_event_loop_create_default() != ESP_OK) {
        ESP_LOGE(TAG, "Event Loop Failed. Restarting...");
        esp_restart();
    }

    // 4. Register State Monitor
    ESP_ERROR_CHECK(esp_event_handler_register(OPENTHREAD_EVENT, 
                                               ESP_EVENT_ANY_ID, 
                                               on_thread_state_changed, NULL));

    // 5. Start UART Task (MUST be before thread_init blocks)
    uart_rx_init();

    // 6. Start Thread
    ESP_LOGI(TAG, "Initializing Thread Stack...");
    
    // Note: thread_init() contains the main loop call and will NOT return.
    thread_init(); 
}