//////// Standard Header file for the PIC16C76 device ////////
#device PIC16C76
#nolist
/////////////////////////////// I/O definitions for INPUT() and OUTPUT_xxx()
#define PIN_A0  40
#define PIN_A1  41
#define PIN_A2  42
#define PIN_A3  43
#define PIN_A4  44
#define PIN_A5  45

#define PIN_B0  48
#define PIN_B1  49
#define PIN_B2  50
#define PIN_B3  51
#define PIN_B4  52
#define PIN_B5  53
#define PIN_B6  54
#define PIN_B7  55

#define PIN_C0  56
#define PIN_C1  57
#define PIN_C2  58
#define PIN_C3  59
#define PIN_C4  60
#define PIN_C5  61
#define PIN_C6  62
#define PIN_C7  63

/////////////////////////////// Useful defines
#define FALSE 0
#define TRUE 1

#define BYTE int
#define BOOLEAN short int

#define getc getch
#define getchar getch
#define puts(s) {printf(s); putchar(13); putchar(10);}
#define putc putchar

/////////////////////////////// Constants used for RESTART_CAUSE()
#define WDT_FROM_SLEEP  0
#define WDT_TIMEOUT     8
#define MCLR_FROM_SLEEP 16
#define NORMAL_POWER_UP 24
/////////////////////////////// Constants used for SETUP_COUNTERS()
#define RTCC_INTERNAL   0
#define RTCC_EXT_L_TO_H 32
#define RTCC_EXT_H_TO_L 48
#define RTCC_DIV_2      0
#define RTCC_DIV_4      1
#define RTCC_DIV_8      2
#define RTCC_DIV_16     3
#define RTCC_DIV_32     4
#define RTCC_DIV_64     5
#define RTCC_DIV_128    6
#define RTCC_DIV_256    7
#define WDT_18MS        8
#define WDT_36MS        9
#define WDT_72MS       10
#define WDT_144MS      11
#define WDT_288MS      12
#define WDT_576MS      13
#define WDT_1152MS     14
#define WDT_2304MS     15
#define L_TO_H              0x40
#define H_TO_L                 0

#define RTCC_ZERO           0x0B20    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_RTCC            0x0B20    // Used for ENABLE/DISABLE INTERRUPTS
#define RB_CHANGE           0x0B08    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_RB              0x0B08    // Used for ENABLE/DISABLE INTERRUPTS
#define EXT_INT             0x0B10    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_EXT             0x0B10    // Used for ENABLE/DISABLE INTERRUPTS

#define GLOBAL              0x0BC0    // Used for ENABLE/DISABLE INTERRUPTS
///////////////////////////////////// Constants used for Timer1 and Timer2
#define T1_DISABLED         0
#define T1_INTERNAL         5
#define T1_EXTERNAL         7
#define T1_EXTERNAL_SYNC    3
#define T1_CLK_OUT          8
#define T1_DIV_BY_1         0
#define T1_DIV_BY_2         0x10
#define T1_DIV_BY_4         0x20
#define T1_DIV_BY_8         0x30
#byte   TIMER_1_LOW=        0x0e
#byte   TIMER_1_HIGH=       0x0f
#define T2_DISABLED         0
#define T2_DIV_BY_1         4
#define T2_DIV_BY_4         5
#define T2_DIV_BY_16        6
#byte   TIMER_2=            0x11

#define INT_TIMER1          0x8C01    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_TIMER2          0x8C02    // Used for ENABLE/DISABLE INTERRUPTS

//////////////////////////////////// Constants used for SETUP_CCP1()
#define CCP_OFF                         0
#define CCP_CAPTURE_FE                  4
#define CCP_CAPTURE_RE                  5
#define CCP_CAPTURE_DIV_4               6
#define CCP_CAPTURE_DIV_16              7
#define CCP_COMPARE_SET_ON_MATCH        8
#define CCP_COMPARE_CLR_ON_MATCH        9
#define CCP_COMPARE_INT                 0xA
#define CCP_COMPARE_RESET_TIMER         0xB
#define CCP_PWM                         0xC
#define CCP_PWM_PLUS_1                  0x1c
#define CCP_PWM_PLUS_2                  0x2c
#define CCP_PWM_PLUS_3                  0x3c
long CCP_1;
#byte   CCP_1    =                      0x15
#byte   CCP_1_LOW=                      0x15
#byte   CCP_1_HIGH=                     0x16

#define INT_CCP1            0x8C04    // Used for ENABLE/DISABLE INTERRUPTS

//////////////////////////////////// Constants used for SETUP_CCP2()
long CCP_2;
#byte   CCP_2    =                      0x1B
#byte   CCP_2_LOW=                      0x1B
#byte   CCP_2_HIGH=                     0x1C

#define INT_CCP2            0x8D01    // Used for ENABLE/DISABLE INTERRUPTS

//////////////////////////////////// Constants used in SETUP_SSP()
#define SPI_MASTER       0x20
#define SPI_SLAVE        0x24
#define SPI_L_TO_H       0
#define SPI_H_TO_L       0x10
#define SPI_CLK_DIV_4    0
#define SPI_CLK_DIV_16   1
#define SPI_CLK_DIV_64   2
#define SPI_CLK_T2       3
#define SPI_SS_DISABLED  1
#define SPI_SAMPLE_AT_END 0x80  // Only for some parts
#define SPI_XMIT_L_TO_H  0x40   // Only for some parts
#define INT_SSP             0x8C08    // Used for ENABLE/DISABLE INTERRUPTS



#define INT_RDA             0x8C20    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_TBE             0x8C10    // Used for ENABLE/DISABLE INTERRUPTS

///////////////////////////////////// Constants used for SETUP_ADC_PORTS()
#define ALL_ANALOG             0
#define ANALOG_RA3_REF         1
#define RA0_RA1_RA3_ANALOG     4
#define RA0_RA1_ANALOG_RA3_REF 5
#define NO_ANALOGS             6
///////////////////////////////////// Constants used for SETUP_ADC()
#define ADC_OFF                0
#define ADC_CLOCK_DIV_2        1
#define ADC_CLOCK_DIV_8     0x41
#define ADC_CLOCK_DIV_32    0x81
#define ADC_CLOCK_INTERNAL  0xc1

#define ADC_DONE            0x8C40    // Used for ENABLE/DISABLE INTERRUPTS
#define INT_ADC             0x8C40    // Used for ENABLE/DISABLE INTERRUPTS

#list
