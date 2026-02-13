#include "commissioner.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "openthread/commissioner.h"
#include "openthread/instance.h"
#include <stdio.h>

static const char *TAG = "COMMISSIONER";

// Helper: readable state
static const char *commissioner_state_to_str(otCommissionerState state)
{
    switch (state) {
        case OT_COMMISSIONER_STATE_DISABLED: return "DISABLED";
        case OT_COMMISSIONER_STATE_PETITION: return "PETITIONING";
        case OT_COMMISSIONER_STATE_ACTIVE:   return "ACTIVE";
        default:                             return "UNKNOWN";
    }
}

static void commissioner_state_cb(otCommissionerState state, void *context)
{
    // Single line log to avoid BLE congestion
    ESP_LOGI(TAG, "STATE_CHANGED: %s", commissioner_state_to_str(state));
}

static void commissioner_joiner_cb(otCommissionerJoinerEvent event,
                                   const otJoinerInfo *info,
                                   const otExtAddress *joiner_id,
                                   void *context)
{
    char joiner_id_str[17] = "UNKNOWN"; // 16 chars + null

    if (joiner_id) {
        snprintf(joiner_id_str, sizeof(joiner_id_str), 
                 "%02X%02X%02X%02X%02X%02X%02X%02X",
                 joiner_id->m8[0], joiner_id->m8[1], joiner_id->m8[2], joiner_id->m8[3],
                 joiner_id->m8[4], joiner_id->m8[5], joiner_id->m8[6], joiner_id->m8[7]);
    }

    // CRITICAL FIX: concise, single-line logs for BLE reliability
    switch (event) {
        case OT_COMMISSIONER_JOINER_START:
            ESP_LOGI(TAG, "JOINER_EVENT START %s", joiner_id_str);
            break;
            
        case OT_COMMISSIONER_JOINER_CONNECTED:
            ESP_LOGI(TAG, "JOINER_EVENT CONNECTED %s", joiner_id_str);
            break;
            
        case OT_COMMISSIONER_JOINER_FINALIZE:
            ESP_LOGI(TAG, "JOINER_EVENT FINALIZE %s", joiner_id_str);
            break;
            
        case OT_COMMISSIONER_JOINER_END:
            ESP_LOGI(TAG, "JOINER_EVENT END %s", joiner_id_str);
            break;
            
        case OT_COMMISSIONER_JOINER_REMOVED:
            // This is the specific log you were missing!
            ESP_LOGI(TAG, "JOINER_EVENT REMOVED %s", joiner_id_str);
            break;
            
        default:
            ESP_LOGW(TAG, "JOINER_EVENT UNKNOWN %d", event);
            break;
    }
}

void commissioner_start(void)
{
    otInstance *instance = esp_openthread_get_instance();
    otCommissionerState state = otCommissionerGetState(instance);
    
    if (state == OT_COMMISSIONER_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Commissioner already ACTIVE");
        return;
    }

    // Always re-register callbacks to ensure we catch events
    otError err = otCommissionerStart(instance, commissioner_state_cb, commissioner_joiner_cb, NULL);

    if (err == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "Commissioner Start: OK");
    } else {
        ESP_LOGE(TAG, "Commissioner Start: FAILED %d", err);
    }
}

void commissioner_stop(void)
{
    otInstance *instance = esp_openthread_get_instance();
    otCommissionerStop(instance);
    ESP_LOGI(TAG, "Commissioner Stopped");
}

bool commissioner_is_active(void)
{
    otInstance *instance = esp_openthread_get_instance();
    return (otCommissionerGetState(instance) == OT_COMMISSIONER_STATE_ACTIVE);
}