idf_component_register(SRCS "src/esp_launcher.c" 
                             "src/launcher_installer.c"
                             "src/launcher_storage.c"
                             "src/launcher_ui.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash fatfs spi_flash app_update driver)
