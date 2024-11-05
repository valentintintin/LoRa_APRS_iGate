#ifndef BOARD_PINOUT_H_
#define BOARD_PINOUT_H_

    #define HAS_LLCC68
    #define HAS_DISPLAY
    #define HAS_1W_LORA

    #undef  OLED_SDA
    #undef  OLED_SCL
    #undef  OLED_RST

    #define OLED_SDA                21
    #define OLED_SCL                22
    #define OLED_RST                -1      // Reset pin # (or -1 if sharing Arduino reset pin)

    #define RADIO_SCLK_PIN          18
    #define RADIO_MISO_PIN          19
    #define RADIO_MOSI_PIN          23
    #define RADIO_CS_PIN            5
    #define RADIO_RST_PIN           27
    #define RADIO_DIO1_PIN          12
    #define RADIO_BUSY_PIN          14
    #define RADIO_RXEN              32
    #define RADIO_TXEN              25

#endif