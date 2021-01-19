/* LCD driver for PICDEM-NET board 
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */
/*
** v0.01 JPB 7/12/00  Adapted from LCD188.C v0.04
** v0.02 JPB 27/12/00 Adapted for new (v3) PICDEM.net board
*/
                   
#define LCD_FUJI 1  // Set non-zero for Fuji FCS2314AK, zero for Hitachi

#if OLD_BOARD
#BIT    LCD_E        = PORTA.1
#BIT    LCD_RD       = PORTA.2
#BIT    LCD_RS       = PORTA.3
#BYTE   LCD_DATA     = PORTD

#else
struct {
    BYTE data:4;
    BYTE regsel:1;
    BYTE read:1;
} LCD_PORT;
#BYTE LCD_PORT = 8                                
#BIT    LCD_E        = PORTA.5
#define LCD_RD       LCD_PORT.read
#define LCD_RS       LCD_PORT.regsel
#define LCD_DATA     LCD_PORT.data
#endif

#define DATA_TO_LCD    set_tris_d(ALL_OUT)
#define DATA_FROM_LCD  set_tris_d(0x0f)

#if LCD_FUJI
#define LCD_SETPOS   0x80
#define LCD_MODE     0x28
#else
#define LCD_SETPOS   0x80
#endif
#define LCD_LINE2    0x40

BOOL disp_lcd, disp_serial;      // Flags to enable display O/Ps

/* Local prototypes */
void lcd_cmd(BYTE b);
void lcd_char(BYTE b);
void lcd_byte(BYTE &b);
void lcd_nybble(BYTE b);

/* Display handler; redirects to LCD and/or serial */
void displays(BYTE b)
{                    
    if (disp_lcd)
    {
        if (b == '\r')
            lcd_cmd(LCD_SETPOS);
        else if (b == '\n')
            lcd_cmd(LCD_SETPOS + LCD_LINE2);
        else
            lcd_char(b);
    }
    if (disp_serial)
    {
        if (b == '\n')
            putchar('\r');
        putchar(b);
    }
}

/* Display a byte in unsigned decimal format */
void disp_decbyte(BYTE b)
{
    printf(displays, "%u", b);
}

/* Initialise the LCD */
void init_lcd(void)
{
    int i;
    
    LCD_E = 0;                      /* Clear LCD clock line */
    DATA_FROM_LCD;                  /* Ensure RS and RD lines are O/Ps */
    LCD_RD = LCD_RS = 0;
    delay_ms(15);                   /* Ensure LCD is stable after power-up */
#if LCD_FUJI                                          
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
#else    
    for (i=0; i<4; i++)             /* Force into 8-bit mode */
    {
        lcd_nybble(0x3);
        delay_ms(5);
    }
    lcd_cmd(0x28);                  /* Set 4-bit mode, 2 lines, 5x7 dots */
    lcd_cmd(0x06);                  /* Incrementing cursor, not horiz scroll */
    lcd_cmd(0x0e);                  /* Display on, cursor on, not blinking */
    lcd_cmd(0x01);                  /* Clear display, home cursor */
#endif    
}

/* Go to an X-Y position on the display, top left is 1, 1 */
void lcd_gotoxy(BYTE x, BYTE y)
{                       
    if (y != 1)
        x += LCD_LINE2;
    lcd_cmd(LCD_SETPOS - 1 + x);
}

/* Send a command byte to the LCD as two nybbles */       
void lcd_char(BYTE b)
{               
    DATA_TO_LCD;
    LCD_RD = 0;
    LCD_RS = 1;
    lcd_byte(b);
}

/* Send a command byte to the LCD as two nybbles */       
void lcd_cmd(BYTE b)
{             
    DATA_TO_LCD;
    LCD_RD = LCD_RS = 0;
    lcd_byte(b);
    if ((b & 0xfc) == 0)
        delay_ms(2);
}

/* Send a command byte to the LCD as two nybbles */       
void lcd_byte(BYTE &b)
{
    lcd_nybble(b >> 4);
    lcd_nybble(b);
    DATA_FROM_LCD;
    delay_us(40);
}

/* Send a command byte to the LCD as two nybbles */       
void lcd_nybble(BYTE b)
{
    DATA_TO_LCD;
    LCD_E = 1;
    LCD_DATA = b;
    LCD_E = 0;
}

/* EOF */
