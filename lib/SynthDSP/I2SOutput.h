#pragma once
#include <Arduino.h>
#include <driver/i2s_std.h>

class I2SOutput {
private:
    i2s_chan_handle_t tx_chan;

public:
    // Expose the tx_chan so audioTask can write blocks directly
    i2s_chan_handle_t getTxChan() { return tx_chan; }

    bool begin(int bckPin, int wsPin, int dataPin, uint32_t sampleRate = 48000) {
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        if (i2s_new_channel(&chan_cfg, &tx_chan, NULL) != ESP_OK) {
            return false;
        }

        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate),
            // REVERTED TO 16-BIT
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)bckPin,
                .ws = (gpio_num_t)wsPin,
                .dout = (gpio_num_t)dataPin,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
            }
        };

        std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_PLL_160M; 

        if (i2s_channel_init_std_mode(tx_chan, &std_cfg) != ESP_OK) return false;
        if (i2s_channel_enable(tx_chan) != ESP_OK) return false;

        return true;
    }
};