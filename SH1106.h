#ifndef __SH1106
#define __SH1106

#include <stdint.h>
#include <stdlib.h>
#include <hardware/dma.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_HEIGHT_BYTE    (OLED_HEIGHT / 8)
#define OLED_WIDTH_BYTE     (OLED_WIDTH / 8)
#define OLED_SIZE           (OLED_HEIGHT * OLED_WIDTH) 
#define OLED_SIZE_BYTE      (OLED_SIZE / 8)

#define PIN_SCL PICO_DEFAULT_I2C_SCL_PIN 
#define PIN_SDA PICO_DEFAULT_I2C_SDA_PIN

#define OLED_I2C_ADDR 0x3C
#define OLED_BAUD (1.5 * 1000000)

//Commands
#define OLED_DISPLAY_OFF                _u(0xAE)
#define OLED_DISPLAY_ON                 _u(0xAF)

#define OLED_SET_VPP                    _u(0x32)
#define OLED_SET_COL_LOW                _u(0x02)
#define OLED_SET_COL_HIGH               _u(0x10)
#define OLED_SET_PAGE_ADDR              _u(0xB0)
#define OLED_SET_SEG_REMAP              _u(0xA1)
#define OLED_SET_COM_SCAN_DIR           _u(0xC8)
#define OLED_SET_REVERSE_NORMAL         _u(0xA6)
#define OLED_SET_DISPLAY_START_LINE     _u(0x40)

#define OLED_SELECT_VCOMH               _u(0xDB)
#define OLED_SELECT_OSC_DIV             _u(0xD5)
#define OLED_SELECT_COM_PINS            _u(0xDA)
#define OLED_SELECT_MUL_RATION          _u(0xA8)
#define OLED_SELECT_DISPLAY_OFFSET      _u(0xD3)
#define OLED_SELECT_CONTRACT_CONTROL    _u(0x81)
#define OLED_SELECT_PRE_CHARGE_PERIOD   _u(0xD9)

#define OLED_PAGES                      _u(8)
#define FRAME_NUM                          2

dma_channel_config OLED_dma_config;
int OLED_dma_chan;

typedef struct{
    uint8_t frame[OLED_SIZE_BYTE];
}frame;

static frame frameBuff[FRAME_NUM];

typedef uint8_t (*func)(uint16_t);


void OLED_Init(void);
static inline void OLED_initFrame(uint8_t *frame);
static void OLED_Clear();
static void OLED_RenderFrame(uint8_t *frame);
static inline void OLED_RenderFrame_DMA(uint8_t *frame);
static inline void OLED_RenderFrame_DMA_Clear(uint8_t *frame);
void OLED_WriteCmdList(uint8_t *list,unsigned int num);
void OLED_CheckFrame_print(const uint8_t *frame);
static inline int OLED_WriteCmd(uint8_t cmd);
static inline int_fast16_t OLED_GetFontIndex(uint8_t ch);
static void OLED_WriteChar(uint8_t *frame, int_fast16_t x, int_fast16_t y, uint8_t ch);
static void OLED_DrawFun(uint8_t *frame,func f,uint8_t x);
static void OLED_WriteString(uint8_t *frame, int_fast16_t x, int_fast16_t y, char *str);
static void OLED_RenderArray(uint8_t *buf,uint16_t num);
static inline void OLED_Set_Page(uint8_t page);
static inline void OLED_Fill_Screen_Pure(uint8_t clo);
static inline void OLED_setPixel(uint8_t *frame, int_fast16_t x, int_fast16_t y, uint8_t on);
static inline void OLED_DrawLine(uint8_t *frame, int_fast16_t x0, int_fast16_t y0, int_fast16_t x1, int_fast16_t y1, uint8_t on);

static inline void OLED_DMA_INIT();

static inline int_fast16_t tool_Fast_abs(int_fast16_t t);
#endif // !__SH1106
