set(srcs
    naphome_test_suite.c
    drivers/sht30_driver.c
    drivers/sgp30_driver.c
    drivers/bh1750_driver.c
    drivers/scd30_driver.c
    web_server.c
    )

set(requires
    hardware_driver
    esp_http_client
    esp_http_server
    mbedtls
    nvs_flash
    esp_wifi
    esp_netif
    esp_event
    driver
    esp-tls
    json
    mdns
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS . drivers include
                       REQUIRES ${requires}
                       EMBED_FILES 
                           "offline_welcome.wav"
                           "Time.mp3")

component_compile_options(-w)
