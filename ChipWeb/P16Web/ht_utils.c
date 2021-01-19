// Utilities for Hitech PICmicro compiler, to emulate CCS functions
//
// Approximate msec delay
void delay_ms(unsigned char val)
{
    unsigned char i;

    while (val--)
    {
        for (i=0; i<4; i++)
            delay_us(250);
    }
}

// Send a char to the serial port
void serial_putch(unsigned char b)
{
    while (!TXIF)
        ;
    TXREG = b;
}

// Send a string to the serial port
void put_ser(const char *str)
{
    char c;

    while ((c = *str++) != 0)
        serial_putch(c);
}

// Output a string to serial/LCD/network
void putstr(const char *str)
{
    char c;

    while ((c = *str++) != 0)
        putch(c);
}

// Serial I/O routines
void init_serial(void)
{
    SPBRG = (CPU_CLK / (16UL * SER_BAUD) - 1);
    TXSTA = 0x04;       // Async, 8 bits, high speed
    RCSTA = 0x90;
    TXEN=1;             // Tx enable
}

/* Read a word value from the ADC */
WORD read_adc(void)
{
    ADCON0 |= 0x04;
    while (ADCON0 & 0x04)
        ;
    return(((WORD)ADRESH << 8) | ADRESL);
}

#define i2c_waitForIdle() while (( SSPCON2 & 0x1F ) | STAT_RW )

/* i2c initialisation */
void init_i2c(void)
{
    TRISC3=1;           // set SCL and SDA pins as inputs
    TRISC4=1;
    SSPCON = 0x38;      // set I2C master mode
    SSPCON2 = 0x00;
    SSPADD = 0x0c;      // 400kHz bus with 20MHz xtal
    SSPSTAT = 0xc0;
    PSPIF=0;
    BCLIF=0;
}

/* Send i2c start bit */
void i2c_start(void)
{
    i2c_waitForIdle();
    SEN=1;
}

/* Send i2c restart */
void i2c_repStart(void)
{
    i2c_waitForIdle();
    RSEN=1;
}

/* Send i2c stop bit */
void i2c_stop(void)
{
    i2c_waitForIdle();
    PEN=1;
}

/* Read byte from i2c */
unsigned char i2c_read(unsigned char ack)
{
    unsigned char b;

    i2c_waitForIdle();
    RCEN=1;
    i2c_waitForIdle();
    b = SSPBUF;
    i2c_waitForIdle();
    if (ack)
        ACKDT=0;
    else
        ACKDT=1;
    ACKEN=1;
    return(b);
}

/* Write byte to i2c, return non-zero if acknowledged */
bit i2c_write(unsigned char b)
{
    i2c_waitForIdle();
    SSPBUF = b;
    return(!ACKSTAT);
}

/* EOF */
