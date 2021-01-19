/* LCD driver for ChipWeb - Copyright (c) Iosoft Ltd 2001
**
** This source code is only licensed for distribution in the Iosoft ChipWeb
** package, and the purchaser of that package is granted the non-exclusive
** right to use the software for personal experimentation only, provided
** that this copyright notice is retained. All other rights are retained by
** Iosoft Ltd.
**
** Redistribution of this source code is not permitted. Binary images derived
** from the source code may only be redistributed if a commercial license is
** obtained; see www.iosoft.co.uk or email license@iosoft.co.uk
**
** The software is supplied 'as-is' for development purposes, without warranty
** of any kind, either expressed or implied, including, but not limited to,
** the implied warranties of merchantability and fitness for purpose.
** In no event will Iosoft Ltd. be liable for damages, including any general,
** special, incidental or consequential damages arising out of the use or
** inability to use the software, including, but not limited to, loss or
** corruption of data, or losses sustained by the developer or third parties,
** or failure of the software to operate with any other software or systems.
** This license shall be governed by the laws of England. */

DEFBIT_5(PORTA, LCD_E)

#define LCD_REGSEL      0x10

#define LCD_PORT        PORTD
#define DATA_TO_LCD     set_tris_d(ALL_OUT)
#define DATA_FROM_LCD   set_tris_d(0x0f)

#define LCD_SETPOS      0x80
#define LCD_MODE        0x28
#define LCD_LINE2       0x40
#define LCD_WIDTH       16

/* Local prototypes */
void lcd_cmd(BYTE b);
void lcd_char(BYTE b);
void lcd_nyb(BYTE b);

/* Initialise the LCD */
void init_lcd(void)
{
    LCD_E = 0;                      /* Clear LCD clock line */
    DATA_FROM_LCD;                  /* Ensure RS and RD lines are O/Ps */
    delay_ms(15);                   /* Ensure LCD is stable after power-up */
    lcd_cmd(LCD_MODE);
    delay_ms(5);
    lcd_cmd(LCD_MODE);
    delay_us(100);
    lcd_cmd(LCD_MODE);
    delay_us(40);
    lcd_cmd(LCD_MODE);              /* Set 4-bit mode, 2 lines, 5x7 dots */
    lcd_cmd(0x04);                  /* Incrementing cursor, not horiz scroll */
    lcd_cmd(0x0e);                  /* Display on, cursor on, not blinking */
    lcd_cmd(0x01);                  /* Clear display, home cursor */
    lcd_cmd(LCD_SETPOS);            /* Data address */
}

/* Go to an X-Y position on the display, top left is 1, 1 */
void lcd_gotoxy(BYTE x, BYTE y)
{
    if (y != 1)
        x += LCD_LINE2;
    lcd_cmd(LCD_SETPOS - 1 + x);
}

/* Clear the given line (1 = top line), and position cursor at start */
void lcd_clearline(BYTE y)
{
    BYTE x, n;

    x = y>1 ? LCD_LINE2 : 0;
    lcd_cmd(LCD_SETPOS + x);
    for (n=0; n<LCD_WIDTH; n++)
        lcd_char(' ');
    lcd_cmd(LCD_SETPOS + x);
}

/* Send a character to the LCD */
void lcd_char(BYTE b)
{
    DATA_TO_LCD;
    lcd_nyb((b>>4) | LCD_REGSEL);
    lcd_nyb((b&0xf) | LCD_REGSEL);
    DATA_FROM_LCD;
    delay_us(40);
}

/* Send a command byte to the LCD */
void lcd_cmd(BYTE b)
{
    DATA_TO_LCD;
    lcd_nyb(b >> 4);
    lcd_nyb(b & 0x0f);
    if ((b & 0xfc) == 0)
        delay_ms(2);
    DATA_FROM_LCD;
    delay_us(40);
}

/* Send a nybble to the LCD, including RD and RS bits */
void lcd_nyb(BYTE b)
{
    LCD_E = 1;
    LCD_PORT = b;
    DELAY_ONE_CYCLE;
    DELAY_ONE_CYCLE;
    LCD_E = 0;
    DELAY_ONE_CYCLE;
}
/* EOF */
