#include <mega128.h>
#include <delay.h>
#include <stdio.h>

// 		RS		PORTG.2
//		RW		PORTG.1
//		E		PORTG.0
//		D7		PORTA.7
//		D6		PORTA.6
//		D5		PORTA.5
//		D4		PORTA.4
//		D3		PORTA.3
//		D2		PORTA.2
//		D1		PORTA.1
//		D0		PORTA.0

#define FUNCSET		0x38
#define ENTMODE		0x06
#define ALLCLR		0x01
#define DISPON		0x0C
#define LINE2		0xC0
#define HOME		0x02

void LCD_init(void);
void LCD_WriteCommand(unsigned char);
void LCD_ShowFreq(unsigned int);
void LCD_WriteData(unsigned char);
void LCD_Busy(void);
void LCD_DrawScale(void);
void LCD_MoveCursor(unsigned char);
void LCD_UpdateScale(float);
void LCD_UpdateSignalStatus(unsigned int);
void LCD_PrintString(char *);
void LCD_ClearLine2(void);