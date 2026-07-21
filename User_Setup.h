#define USER_SETUP_ID 999

// --- CRITICAL ESP32-S3 FLAGS ---
#define CONFIG_IDF_TARGET_ESP32S3 1
#define BOARD_HAS_PSRAM 1          // Crucial when using Octal PSRAM

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_OFF

// --- YOUR SAFE PHYSICAL PINS ---
#define TFT_MISO -1
#define TFT_MOSI 11     // Standard FSPI MOSI
#define TFT_SCLK 12     // Standard FSPI SCLK
#define TFT_CS   10     // Standard FSPI CS
#define TFT_DC   14     // Safe generic GPIO
#define TFT_RST  9      // Safe generic GPIO

#define SPI_FREQUENCY  27000000  

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF