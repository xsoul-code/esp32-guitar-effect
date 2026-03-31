#include <stdio.h>
#include "include/wifi.h"
#include "nvs_flash.h"

void app_main(void)
{

    // Wifi section
    nvs_flash_init();
    wifi_init_sta();

}
