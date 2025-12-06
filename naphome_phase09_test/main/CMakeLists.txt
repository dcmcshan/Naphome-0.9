set(srcs
    naphome_test_suite.c
    drivers/sht30_driver.c
    drivers/sgp30_driver.c
    )

set(requires
    hardware_driver
    esp_http_client
    mbedtls
    nvs_flash
    esp_wifi
    esp_netif
    esp_event
    driver
    esp-tls
    json
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS . drivers include
                       REQUIRES ${requires}
                       EMBED_FILES 
                           "offline_welcome.wav"
                           "frequencies_fear1.mp3")

component_compile_options(-w)
