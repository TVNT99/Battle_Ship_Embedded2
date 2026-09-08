/******************************************************************************
 * @file     EBI_LCD_Module.c
 *
 * @brief    Subfunctins to display contents on LCD module thru EBI interface
 * @version  1.0.0
 *
 * @copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "NuMicro.h"

#include "EBI_LCD_Module.h"

/* LCD Module "RS" */
#define PH_CTRL_RS      1
#if PH_CTRL_RS
#define SET_RS          PH3 = 1;
#define CLR_RS          PH3 = 0;
#endif

/* LCD Module "RESET" */
#define SET_RST         PB6 = 1;
#define CLR_RST         PB6 = 0;

volatile    uint8_t     Timer3_flag = 0;
volatile    uint8_t     Timer3_cnt = 0;

volatile    uint32_t    g_u32AdcIntFlag_TP;

/**
 * @brief       TMR3 IRQ handler
 *
 * @param       None
 *
 * @return      None
 *
 * @details     The Timer3 default IRQ, declared in startup_M480.s
 */
void TMR3_IRQHandler(void)
{
    /* Clear timer interrupt flag */
    TIMER3->INTSTS |= TIMER_INTSTS_TIF_Msk;

    /* Set Timer3_flag = 1 */
    Timer3_flag = 1;

    /* Timer3_cnt + 1 */
    Timer3_cnt = Timer3_cnt + 1;

}

/**
 * @brief       Initial Timer3
 *
 * @param       None
 *
 * @return      None
 *
 * @details     To initialize Timer3 as periodic mode @100ms and enable interrupt
 */
void Timer3_Init(void)
{
    /* 1/12MHz * 1200000 = 100ms */
    TIMER3->CMP = 1200000;

    /* Enable Timer3 interrupt and set Timer3 as Periodic Mode, prescale = 0 (/1) */
    TIMER3->CTL = TIMER_CTL_INTEN_Msk | TIMER_PERIODIC_MODE;

    /* Enable Timer3 IRQ */
    NVIC_EnableIRQ(TMR3_IRQn);

    /* Enable Timer3 */
    TIMER3->CTL |= TIMER_CTL_CNTEN_Msk;

    /* Clear Timer3_flag */
    Timer3_flag = 0;

    /* Reset Timer3_cnt */
    Timer3_cnt = 0;

}

/**
 * @brief       Write command to LCD
 *
 * @param       cmd: the command will be written
 *
 * @return      None
 *
 * @details     To write command to LCD thru the EBI interface
 */
#if PH_CTRL_RS
void LCD_WR_REG(uint16_t cmd)
{
    CLR_RS
    EBI0_WRITE_DATA16(0x00000000, cmd);
    SET_RS
}
#else
void LCD_WR_REG(uint16_t cmd)
{
    EBI0_WRITE_DATA16(0x00000000, cmd);
}
#endif

/**
 * @brief       Write data to LCD
 *
 * @param       dat: the data will be written
 *
 * @return      None
 *
 * @details     To write data to LCD thru the EBI interface
 */
void LCD_WR_DATA(uint16_t dat)
{
    EBI0_WRITE_DATA16(0x00030000, dat);

}

/**
 * @brief       Read back data from LCD
 *
 * @param       None
 *
 * @return      The read back data
 *
 * @details     To read data from LCD thru the EBI interface
 */
uint16_t LCD_RD_DATA(void)
{
    return EBI0_READ_DATA16(0x00030000);

}

/**
 * @brief       Set LCD window
 *
 * @param       x_s: the start position in the x-direction of window
 *              x_e: the end position in the x-direction of window
 *              y_s: the start position in the y-direction of window
 *              y_e: the end position in the y-direction of window
 * @return      None
 *
 * @details     To set the window area of LCD module
 */
void LCD_SetWindow(uint16_t x_s, uint16_t x_e, uint16_t y_s, uint16_t y_e)
{
    /* X range */
    LCD_WR_REG(0x2A);
    LCD_WR_DATA(x_s>>8);
    LCD_WR_DATA(x_s);
    LCD_WR_DATA(x_e>>8);
    LCD_WR_DATA(x_e);

    /* Y range */
    LCD_WR_REG(0x2B);
    LCD_WR_DATA(y_s>>8);
    LCD_WR_DATA(y_s);
    LCD_WR_DATA(y_e>>8);
    LCD_WR_DATA(y_e);

    /* Memory write */
    LCD_WR_REG(0x2C);

}

/**
 * @brief       Initial ILI9341 LCD driver
 *
 * @param       None
 *
 * @return      None
 *
 * @details     To initialize the ILI9341 LCD driver
 */
void ILI9341_Initial(void)
{
    /* Hardware reset */
    SET_RST;
    CLK_SysTickDelay(5000);     // Delay 5ms

    CLR_RST;
    CLK_SysTickDelay(20000);    // Delay 20ms

    SET_RST;
    CLK_SysTickDelay(40000);    // Delay 40ms

    /* Initial control registers */
    LCD_WR_REG(0xCB);
    LCD_WR_DATA(0x39);
    LCD_WR_DATA(0x2C);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x34);
    LCD_WR_DATA(0x02);

    LCD_WR_REG(0xCF);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0xC1);
    LCD_WR_DATA(0x30);

    LCD_WR_REG(0xE8);
    LCD_WR_DATA(0x85);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x78);

    LCD_WR_REG(0xEA);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xED);
    LCD_WR_DATA(0x64);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x81);

    LCD_WR_REG(0xF7);
    LCD_WR_DATA(0x20);

    LCD_WR_REG(0xC0);
    LCD_WR_DATA(0x23);

    LCD_WR_REG(0xC1);
    LCD_WR_DATA(0x10);

    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x3e);
    LCD_WR_DATA(0x28);

    LCD_WR_REG(0xC7);
    LCD_WR_DATA(0x86);

    LCD_WR_REG(0x36);
    LCD_WR_DATA(0x48);

    LCD_WR_REG(0x3A);
    LCD_WR_DATA(0x55);

    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x18);

    LCD_WR_REG(0xB6);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x82);
    LCD_WR_DATA(0x27);

    LCD_WR_REG(0xF2);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0x26);
    LCD_WR_DATA(0x01);

    LCD_WR_REG(0xE0);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x31);
    LCD_WR_DATA(0x2B);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x0E);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x4E);
    LCD_WR_DATA(0xF1);
    LCD_WR_DATA(0x37);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x0E);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xE1);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x0E);
    LCD_WR_DATA(0x14);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x31);
    LCD_WR_DATA(0xC1);
    LCD_WR_DATA(0x48);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x31);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x0F);

    LCD_WR_REG(0x11);
    CLK_SysTickDelay(200000);   // Delay 200ms

    LCD_WR_REG(0x29);           //Display on

    printf("Initial ILI9341 LCD Module done.\n\n");

}

/**
 * @brief       Put an 8x16 character to LCD screen
 *
 * @param       x: the start position in the x-direction
 *              y: the start position in the y-direction
 *              c: the character of ASCII
 *              fcolor: the front color
 *              bcolor: the background color
 * @return      None
 *
 * @details     To show an 8x16 character on the LCD screen
 */
void LCD_PutChar8x16(uint16_t x, uint16_t y, uint8_t c, uint32_t fColor, uint32_t bColor)
{
    uint32_t    i, j;
    uint8_t     m;

    for(i=0; i<16; i++) {
        m = Font8x16[c*16+i];
        LCD_SetWindow(x, x+7, y+i, y+i);
        for(j=0; j<8; j++) {
            if((m & 0x80) == 0x80) {
                LCD_WR_DATA(fColor);
            } else {
                LCD_WR_DATA(bColor);
            }
            m <<= 1;
        }
    }

}

/**
 * @brief       Put a string to LCD screen
 *
 * @param       x: the start position in the x-direction
 *              y: the start position in the y-direction
 *              *s: the point of string
 *              fcolor: the front color
 *              bcolor: the background color
 * @return      None
 *
 * @details     To show a string on the LCD screen
 */
void LCD_PutString(uint16_t x, uint16_t y, uint8_t *s, uint32_t fColor, uint32_t bColor)
{
    uint8_t     l = 0;

    while(*s) {
        if(*s < 0x80) {
            LCD_PutChar8x16(x+l*8, y, *s, fColor, bColor);
            s++;
            l++;
        }
    }

}

/**
 * @brief       Put a larger 16x32 number (0~9) to LCD screen
 *
 * @param       x: the start position in the x-direction
 *              y: the start position in the y-direction
 *              c: the number 0~9
 *              fcolor: the front color
 *              bcolor: the background color
 * @return      None
 *
 * @details     To show a larger 16x32 number on the LCD screen
 */
void LCD_PutChar16x32(uint16_t x, uint16_t y, uint16_t c, uint32_t fColor, uint32_t bColor)
{
    uint32_t    i, j;
    uint16_t    m;

    for(i=0; i<32; i++) {
        m = Font16x32[c*32+i];
        LCD_SetWindow(x, x+15, y+i, y+i);
        for(j=0; j<16; j++) {
            if((m & 0x8000) == 0x8000) {
                LCD_WR_DATA(fColor);
            } else {
                LCD_WR_DATA(bColor);
            }
            m <<= 1;
        }
    }

}

/**
 * @brief       Blank an area of LCD screen
 *
 * @param       X: the X start position of this area, 0~239
 *              Y: the Y start position of this area, 0~319
 *              W: the width of this area, X+W < 240
 *              H: the height of this area, Y+H < 320
 *              color: the color will be filled into this area
 * @return      None
 *
 * @details     To fill a fixed color into an area on the LCD screen
 */
void LCD_BlankArea(uint16_t X, uint16_t Y, uint16_t W, uint16_t H, uint16_t color)
{
    uint16_t    i, j;

    LCD_SetWindow(X, X+W-1, Y, Y+H-1);
    for(j=0; j<H; j++) {
        for(i=0; i<W; i++)
            LCD_WR_DATA(color);
    }

}

/**
 * @brief       Get X position from touch panel thru the ADC input
 *
 * @param       None
 *
 * @return      The X position on LCD screen
 *
 * @details     To get the X position when finger touching on the LCD screen
 */
uint16_t Get_TP_X(void)
{
    /* Touch screen not implemented for Battle Ship */
    return 0;
}

    /*=== Calculate the X position ===*/
    X_pos = (x_adc_in - 170)/2.8;	// range of x_adc_in is [0:842]

    if(X_pos >= (LCD_W - 1)) X_pos = LCD_W - 1;

    printf("Position X: %d\n", X_pos);
    return X_pos;

}

/**
 * @brief       Get Y position from touch panel thru the ADC input
 *
 * @param       None
 *
 * @return      The Y position on LCD screen
 *
 * @details     To get the Y position when finger touching on the LCD screen
 */
uint16_t Get_TP_Y(void) { return 0; }
















































/**
 * @brief       ADC01 IRQ handler
 *
 * @param       None
 *
 * @return      None
 *
 * @details     The ADC01 default IRQ, declared in startup_M480.s
 */
void EADC01_IRQHandler(void)
{
    /* Clear the A/D ADINT1 interrupt flag */
    EADC_CLR_INT_FLAG(EADC, EADC_STATUS2_ADIF1_Msk);

    g_u32AdcIntFlag_TP = 1;

}

