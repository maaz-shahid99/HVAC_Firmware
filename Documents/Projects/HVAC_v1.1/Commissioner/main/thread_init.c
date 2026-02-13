#include "thread_init.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_event.h"
#include "esp_vfs_eventfd.h"
#include "nvs_flash.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_ot_config.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"
#include "esp_random.h" // Required for manual key generation

static const char *TAG = "THREAD";

static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));
    return netif;
}

void form_new_network(const char *network_name) {
    otInstance *instance = esp_openthread_get_instance();
    
    if (esp_openthread_lock_acquire(pdMS_TO_TICKS(5000))) {
        ESP_LOGI(TAG, "Creating New Network Dataset...");
        
        // 1. Wipe current dataset
        otInstanceFactoryReset(instance); 
        
        otOperationalDataset dataset;
        memset(&dataset, 0, sizeof(otOperationalDataset));

        // 2. Set Network Name
        otNetworkName name;
        snprintf(name.m8, sizeof(name), "%s", network_name);
        dataset.mNetworkName = name;
        dataset.mComponents.mIsNetworkNamePresent = true;

        // 3. Generate Random Network Key (Master Key)
        otNetworkKey networkKey;
        esp_fill_random(networkKey.m8, sizeof(networkKey.m8));
        dataset.mNetworkKey = networkKey;
        dataset.mComponents.mIsNetworkKeyPresent = true;

        // 4. Generate Random Extended PAN ID
        otExtendedPanId extPanId;
        esp_fill_random(extPanId.m8, sizeof(extPanId.m8));
        dataset.mExtendedPanId = extPanId;
        dataset.mComponents.mIsExtendedPanIdPresent = true;

        // 5. Generate Random PAN ID
        dataset.mPanId = (otPanId)(esp_random() & 0xFFFF);
        dataset.mComponents.mIsPanIdPresent = true;

        // 6. Set Default Channel (or random)
        dataset.mChannel = 15; 
        dataset.mComponents.mIsChannelPresent = true;

        // 7. Commit as Active Dataset
        otDatasetSetActive(instance, &dataset);
        
        // 8. Bring Up Interface
        otIp6SetEnabled(instance, true);
        otThreadSetEnabled(instance, true);

        printf("NETWORK_FORMED\n");
        fflush(stdout);

        ESP_LOGI(TAG, "Network '%s' Formed. Device is now Leader.", network_name);
        esp_openthread_lock_release();
    }
}

void thread_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS Init failed!");
        return;
    }

    if (esp_netif_init() != ESP_OK) {
        ESP_LOGE(TAG, "Netif Init failed!");
        return;
    }
    
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&(esp_vfs_eventfd_config_t){
        .max_fds = 3,
    }));

    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    if (esp_openthread_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "OT Init failed!");
        return;
    }

    esp_netif_t *ot_netif = init_openthread_netif(&config);
    if (!ot_netif) {
        ESP_LOGE(TAG, "Netif creation failed!");
        return;
    }

    otInstance *instance = esp_openthread_get_instance();
    otIp6SetEnabled(instance, true);
    otThreadSetEnabled(instance, true);

    ESP_LOGI(TAG, "Launching Main Loop");
    esp_openthread_launch_mainloop();
}
