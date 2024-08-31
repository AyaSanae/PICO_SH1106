#include "SH1106.h"
#include "SH1106_font.h"
#include "pico/platform.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "hardware/dma.h"

dma_channel_config OLED_dma_config;
int OLED_dma_chan;

// Function to get the absolute value of a signed integer
static inline int_fast16_t tool_Fast_abs(int_fast16_t t) {
    return (t ^ (t >> 15)) - (t >> 15);
}

// Initialize the OLED display
void OLED_Init(void) {
    uint8_t init_cmds[] = {
        OLED_DISPLAY_OFF,
        OLED_SET_COL_LOW,
        OLED_SET_COL_HIGH,
        OLED_SET_DISPLAY_START_LINE,
        OLED_SET_PAGE_ADDR,
        OLED_SELECT_CONTRACT_CONTROL,
        0xBF,
        OLED_SET_SEG_REMAP,
        OLED_SET_REVERSE_NORMAL,
        OLED_SELECT_MUL_RATION,
        0x3F,
        OLED_SET_COM_SCAN_DIR,
        OLED_SELECT_DISPLAY_OFFSET,
        0x00,
        OLED_SELECT_OSC_DIV,
        0x80,
        OLED_SELECT_PRE_CHARGE_PERIOD,
        0x1F,
        OLED_SELECT_COM_PINS,
        0x12, // 128x64 display setting
        OLED_SELECT_VCOMH,
        0x40
    };  
    OLED_WriteCmdList(init_cmds, count_of(init_cmds)); // Send initialization commands
    sleep_ms(20); // Wait for the display to initialize
    OLED_WriteCmd(OLED_DISPLAY_ON); // Turn on the display

    // Initialize DMA for frame transfer
    OLED_DMA_INIT();
}

// Write a single command to the OLED
static inline int OLED_WriteCmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd}; // Command buffer
    return i2c_write_blocking(i2c0, OLED_I2C_ADDR, buf, 2, false); // Send the command
}

// Write a list of commands to the OLED
static inline void OLED_WriteCmdList(uint8_t *list, unsigned int num) {
    for (unsigned int i = 0; i < num; i++) {
        OLED_WriteCmd(list[i]); // Send each command
    }
}

// Clear the display by filling it with black
void OLED_Clear() {
    OLED_Fill_Screen_Pure(0x00);
}

// Fill the entire screen with a specific color
static inline void OLED_Fill_Screen_Pure(uint8_t color) {
    uint8_t clr_data[2] = {0x40, color}; // Color data for filling
    for (size_t j = 0; j < 8; j++) { // Loop through all pages
        OLED_Set_Page(j);
        for (size_t i = 0; i < OLED_WIDTH; i++) {
            i2c_write_blocking(i2c0, OLED_I2C_ADDR, clr_data, 2, false); // Write color data
        }
    }
}

// Render an array of pixel data to the OLED
void OLED_RenderArray(uint8_t *buf, uint16_t num) {
    if (buf == NULL || num == 0) return; // Check for valid buffer
    
    OLED_Set_Page(0); // Set to the first page

    // If the number of bytes is less than the OLED width
    if (num <= OLED_WIDTH) {
        uint8_t *tmp = malloc(num + 1); // Allocate memory for buffer
        if (tmp == NULL) return; // Check for allocation failure

        tmp[0] = 0x40; // Data command
        memcpy(tmp + 1, buf, num); // Copy data

        i2c_write_blocking(i2c0, OLED_I2C_ADDR, tmp, num + 1, false); // Send data
        free(tmp);
        return;
    }

    uint8_t *tmp = malloc(OLED_WIDTH + 1); // Allocate memory
    if (tmp == NULL) return; // Check for allocation failure
    tmp[0] = 0x40; // Data command
    uint8_t pages = num / OLED_WIDTH; // Calculate full pages
    uint8_t remaining = num % OLED_WIDTH; // Calculate remaining bytes

    for (uint8_t page = 0; page < pages; page++) {
        OLED_Set_Page(page); // Set current page
        memcpy(tmp + 1, buf + page * OLED_WIDTH, OLED_WIDTH); // Copy the full page data
        i2c_write_blocking(i2c0, OLED_I2C_ADDR, tmp, OLED_WIDTH + 1, false); // Send page data
    }

    // Send any remaining data
    if (remaining != 0) {
        OLED_Set_Page(pages); // Set next page
        memcpy(tmp + 1, buf + pages * OLED_WIDTH, remaining); // Copy remaining data
        i2c_write_blocking(i2c0, OLED_I2C_ADDR, tmp, remaining + 1, false); // Send remaining data
    }

    free(tmp); // Free allocated memory
}

// Render the complete frame to the OLED
void OLED_RenderFrame(uint8_t *frame) {
    if (frame == NULL) return; // Check for valid frame

    uint8_t *buf0 = malloc(OLED_WIDTH + 1); // Allocate buffer
    if (buf0 == NULL) return; // Check for allocation failure
    buf0[0] = 0x40; // Data command

    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        OLED_Set_Page(page); // Set to current page
        memcpy(buf0 + 1, frame + page * OLED_WIDTH, OLED_WIDTH); // Copy frame data
        i2c_write_blocking(i2c0, OLED_I2C_ADDR, buf0, OLED_WIDTH + 1, false); // Send the data
    }

    free(buf0); // Free allocated memory
}

// DMA version of rendering the frame
void OLED_RenderFrame_DMA(uint8_t *frame) {
    if (!frame) return; // Check for valid frame

    uint8_t *buf0 = malloc(OLED_WIDTH + 1); // Allocate buffer
    if (!buf0) return; // Check for allocation failure
    buf0[0] = 0x40; // Data command

    dma_channel_configure(
        OLED_dma_chan,
        &OLED_dma_config,
        buf0 + 1, // Write address
        frame,    // Read address
        OLED_WIDTH, // Number of bytes to transfer
        true // Start transfer immediately
    );

    for (uint8_t page = 1; page < OLED_PAGES + 1; page++) {
        OLED_Set_Page(page - 1); // Set current page
        dma_channel_wait_for_finish_blocking(OLED_dma_chan); // Wait for DMA transfer to finish
        i2c_write_blocking(i2c0, OLED_I2C_ADDR, buf0, OLED_WIDTH + 1, false); // Send the data
        dma_channel_set_read_addr(OLED_dma_chan, frame + page * OLED_WIDTH, false); // Set next read address
        dma_channel_set_write_addr(OLED_dma_chan, buf0 + 1, true); // Set buffer write address
    }

    free(buf0); // Free allocated memory
}

// Clear and render the frame via DMA
void OLED_RenderFrame_DMA_Clear(uint8_t *frame) {
    OLED_RenderFrame_DMA(frame); // Render the frame
    OLED_initFrame(frame); // Initialize the frame (clear it)
}

// Set the current page for the OLED
static inline void OLED_Set_Page(uint8_t page) {
    OLED_WriteCmd(OLED_SET_PAGE_ADDR | page); // Set the page address
    OLED_WriteCmd(OLED_SET_COL_LOW); // Set lower column address
    OLED_WriteCmd(OLED_SET_COL_HIGH); // Set higher column address
}

// Initialize the frame buffer to 0 (clear)
void OLED_initFrame(uint8_t *frame) {
    memset(frame, 0x00, OLED_SIZE_BYTE); // Clear the frame buffer
}

// Set a pixel in the frame buffer
void OLED_setPixel(uint8_t *frame, int_fast8_t x, int_fast8_t y, uint8_t on) {
    assert(x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT); // Check pixel bounds
    uint16_t index = (y / 8) * OLED_WIDTH + x; // Calculate index in buffer
    uint8_t *target_pixel_col = &frame[index]; // Get pointer to target pixel column
    if (on) {
        *(target_pixel_col) |= 1 << (y % 8); // Set the pixel
    } else {
        *(target_pixel_col) &= ~(1 << (y % 8)); // Clear the pixel
    }
}

// Draw a line using Bresenham's algorithm
void OLED_DrawLine(uint8_t *frame, int_fast16_t x0, int_fast16_t y0, int_fast16_t x1, int_fast16_t y1, uint8_t on) {
    int_fast16_t dx = tool_Fast_abs(x1 - x0);
    int_fast16_t sx = x0 < x1 ? 1 : -1; // Determine the step direction
    int_fast16_t dy = -tool_Fast_abs(y1 - y0);
    int_fast16_t sy = y0 < y1 ? 1 : -1; // Determine the step direction
    int_fast16_t err = dx + dy; // Error value

    while (true) {
        OLED_setPixel(frame, x0, y0, on); // Set the pixel
        if (x0 == x1 && y0 == y1) break; // If reached the endpoint, break

        int_fast16_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy; // Adjust error value
            x0 += sx; // Move in x direction
        }
        if (e2 <= dx) {
            err += dx; // Adjust error value
            y0 += sy; // Move in y direction
        }
    }
}

// Get the font index for a given character
static inline int_fast16_t OLED_GetFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 1; // Index for uppercase letters
    } else if (ch >= '0' && ch <= '9') {
        return ch - '0' + 27; // Index for digits
    } else return 0; // Default index
}

// Write a character to the frame buffer
void OLED_WriteChar(uint8_t *frame, int_fast16_t x, int_fast16_t y, uint8_t ch) {
    if (x > OLED_WIDTH - 8 || y > OLED_HEIGHT - 8) return; // Check bounds

    y = y / 8; // Adjust y for character height
    ch = toupper(ch); // Convert to uppercase
    int_fast16_t idx = OLED_GetFontIndex(ch); // Get font index
    int_fast16_t fb_idx = y * 128 + x; // Calculate frame buffer index

    for (int i = 0; i < 8; i++) {
        frame[fb_idx++] = font[idx * 8 + i]; // Copy font data to frame
    }
}

// Write a string to the frame buffer
void OLED_WriteString(uint8_t *frame, int_fast16_t x, int_fast16_t y, char *str) {
    if (x > OLED_WIDTH - 8 || y > OLED_HEIGHT - 8) return; // Check bounds
    while (*str) {
        OLED_WriteChar(frame, x, y, *str++); // Write each character
        x += 8; // Move to next character position
    }
}

// Draw a function on the OLED by sampling y values
void OLED_DrawFun(uint8_t *frame, func f, uint8_t x) {
    for (uint8_t i = x; i < 128 - x; i++) {
        uint8_t y_val = f(i); // Get y value from function
        if (y_val >= 0 && y_val < 64) {
            OLED_setPixel(frame, i, y_val, 1); // Set the pixel if within bounds
        }
    }
}

// Draw a circle using the midpoint algorithm
static void OLED_DrawCircle(uint8_t *frame, int16_t centerX, int16_t centerY, uint8_t radius) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t radiusError = 1 - radius; // Initialize the decision parameter

    while (x >= y) {
        // Draw the eight symmetric points of the circle
        OLED_setPixel(frame, centerX + x, centerY + y, 1);
        OLED_setPixel(frame, centerX + y, centerY + x, 1);
        OLED_setPixel(frame, centerX - y, centerY + x, 1);
        OLED_setPixel(frame, centerX - x, centerY + y, 1);
        OLED_setPixel(frame, centerX - x, centerY - y, 1);
        OLED_setPixel(frame, centerX - y, centerY - x, 1);
        OLED_setPixel(frame, centerX + y, centerY - x, 1);
        OLED_setPixel(frame, centerX + x, centerY - y, 1);

        y++;
        if (radiusError < 0) {
            radiusError += 2 * y + 1; // Choose the next point
        } else {
            x--; // Move horizontally
            radiusError += 2 * (y - x + 1); // Adjust decision parameter
        }
    }
}

// Initialize the DMA for OLED rendering
static inline void OLED_DMA_INIT() {
    OLED_dma_chan = dma_claim_unused_channel(1); // Claim an unused DMA channel
    OLED_dma_config = dma_channel_get_default_config(OLED_dma_chan); // Get the default channel config
    channel_config_set_transfer_data_size(&OLED_dma_config, DMA_SIZE_8); // Set data size
    channel_config_set_read_increment(&OLED_dma_config, true); // Enable read address increment
    channel_config_set_write_increment(&OLED_dma_config, true); // Enable write address increment
}
