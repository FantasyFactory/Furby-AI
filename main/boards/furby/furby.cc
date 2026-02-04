#include "wifi_board.h"
#include "audio/codecs/vb6824_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
//#include "led/single_led.h"

//#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "font_awesome_symbols.h"
#include "settings.h"

#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>
#include "assets/lang_config.h"


#define TAG "FurbyBoard"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class CustomBoard : public WifiBoard {
private:
    Button boot_button_;
    Button other_button_;
    VbAduioCodec audio_codec;
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (audio_codec.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
            auto &app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if(count >= 3){
                if (audio_codec.InOtaMode(1) == true) {
                    ESP_LOGI(TAG, "OTA mode, do not enter chat");
                    return;
                }
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnLongPress([this]() {
            if (esp_timer_get_time() > 20 * 1000 * 1000) {
                ESP_LOGI(TAG, "Long press, do not enter OTA mode %ld", (uint32_t)esp_timer_get_time());
                return;
            }
            int ret = audio_codec.OtaStart(0); 
            if(ret == VbAduioCodec::OTA_ERR_NOT_SUPPORT){
                ESP_LOGW(TAG, "Please enable VB6824_OTA_SUPPORT");
            }
        });
        other_button_.OnClick([this]() {
            ESP_LOGI(TAG, "other_button_.OnClick invoked");
        }); 
    }
 
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

#ifdef SSD1306_CONTROLLER
    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }
#endif

#ifdef SSD1315_CONTROLLER
    void InitializeSsd1315Display() {
        // SSD1315 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1315 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1315_config_t ssd1315_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1315_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1315(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1315driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }
#endif

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }

public:
    CustomBoard() : boot_button_(BOOT_BUTTON_GPIO,false,3000), other_button_(OTHER_BUTTON_GPIO), audio_codec(CODEC_TX_GPIO, CODEC_RX_GPIO) {    
        InitializeButtons();
        //InitializeIot();
        InitializeDisplayI2c();
#ifdef SSD1306_CONTROLLER
        InitializeSsd1306Display();
#endif
#ifdef SSD1315_CONTROLLER
        InitializeSsd1315Display();
#endif
        audio_codec.OnWakeUp([this](const std::string& command) {
            if (command == std::string(vb6824_get_wakeup_word())){
                if(Application::GetInstance().GetDeviceState() != kDeviceStateListening){
                    Application::GetInstance().WakeWordInvoke("你好小智");
                }
            }else if (command == "开始配网"){
                ResetWifiConfiguration();
            }
        });
    
    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    void EnterWifiConfigMode() {
        auto& application = Application::GetInstance();
        application.SetDeviceState(kDeviceStateWifiConfiguring);

        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        wifi_ap.SetLanguage(Lang::CODE);
        wifi_ap.SetSsidPrefix("Xiaozhi");
        wifi_ap.Start();

        // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
        std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
        hint += wifi_ap.GetSsid();
        hint += Lang::Strings::ACCESS_VIA_BROWSER;
        hint += wifi_ap.GetWebServerUrl();
        hint += "\n\n";
        
        // 播报配置 WiFi 的提示
        application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);

        vTaskDelay(pdMS_TO_TICKS(3500));

        // 可以释放opus编码器的内存
        
        // Wait forever until reset after configuration
        while (true) {
            int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
            ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
};

DECLARE_BOARD(CustomBoard);
