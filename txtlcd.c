#include <txtlcd.h>

unsigned char lcd_index_old;                                    // LCD�ε����� �������� �����Ѵ�.
unsigned char needle_index_old;                                 // �ٴ��ε����� �������� �����Ѵ�.


// LCD �ʱ�ȭ �Լ�
void LCD_init(void)
{
    DDRG = 0b00000111;                                          // PG0���� PG2���� ��� ����
    DDRA = 0b11111111;                                          // PD0���� PD7���� ��� ����
    PORTG &= 0b11111110;                                        // E = 0
            
    delay_ms(15);                                               // Wait for more than 15 ms
    LCD_WriteCommand(0b00110000);                               // Function set
    delay_ms(5);                                                // Wait for more than 4.1 ms
    LCD_WriteCommand(0b00110000);                               // Function set
    delay_us(100);                                              // Wait for more than 100 ��s
    LCD_WriteCommand(0b00110000);                               // Function set
            
    LCD_WriteCommand(FUNCSET);                                  // ��� ��
    LCD_WriteCommand(DISPON);                                   // ���÷��� ON
    LCD_WriteCommand(ALLCLR);                                   // LCDŬ����
    LCD_WriteCommand(ENTMODE);                                  // ��Ʈ�� ��� ��
}       

// LCD�� �ν�Ʈ������ ���� �Լ�
void LCD_WriteCommand(unsigned char byte)       
{       
    LCD_Busy();                                                 // 2ms��ŭ ���         
            
    PORTA = byte;                                               // ������������ �Է��� �ν�Ʈ���� ���
    PORTG &= 0b11111001;                                        // RS = 0 
                                                                // RW = 0
    delay_us(1);                                                // t_su�� �ּҰ�: 100ns
    PORTG |= 0b00000001;                                        // E = 1
    delay_us(1);                                                // t_w�� �ּҰ�: 300ns
    PORTG &= 0b11111110;                                        // E = 0
} 

// LCD�� �����͸� ���� �Լ�
void LCD_WriteData(unsigned char byte)
{
    LCD_Busy();                                                 // 2ms��ŭ ���     
    
    PORTA = byte;                                               // ������������ �Է��� ������ ���
    PORTG |= 0b00000100;                                        // RS = 1
    PORTG &= 0b11111101;                                        // RW = 0
    delay_us(1);                                                // t_su�� �ּҰ�: 100ns
    PORTG |= 0b00000001;                                        // E = 1
    delay_us(1);                                                // t_w�� �ּҰ�: 300ns
    PORTG &= 0b11111110;                                        // E = 0
}

// LCD�� ���ļ��� ������ִ� �����ƾ 
// �߰��� ���׳� ����� ��ȣ�� ����Ѵ�.
void LCD_ShowFreq(unsigned int freq)                        
{
    char str[10];                                               // ��Ʈ�� �迭�� �����
    LCD_WriteCommand(LINE2);                                    // �ι�° �ٷ� �̵��� ������
    sprintf(str, " %3d.%dMhz  ", freq / 10, freq % 10 );        // ��Ʈ���� ����� ���ڸ� �������ְ�
    LCD_PrintString(str);                                       // LCD�� ����Ѵ� ����� ���ڸ�
    LCD_WriteData(0b10110111);
}          



// LCD���� num��ŭ�� Ŀ���� �̵��ϴ� �����ƾ
void LCD_MoveCursor(unsigned char num)
{
    unsigned char i;
    for(i = 0; i<num; i++){                                     // num��ŭ �ݺ�
        LCD_WriteCommand(0b00010100);                           // Ŀ���� ��ĭ �̵��ϴ� �ν�Ʈ����
    }
}

// �����÷��׸� üũ�ϴ´�� 2ms����ϴ� �����ƾ
void LCD_Busy(void)
{
    delay_ms(2);
}

// LCD�� ���� ������ ������ִ� �����ƾ
void LCD_DrawScale(void)
{
    unsigned char i = 0;
    unsigned char j = 0;
    
    unsigned char scale_pat[8] =  { 0b00000,
                                    0b00000,
                                    0b00000,
                                    0b00000,
                                    0b00100,
                                    0b10101,
                                    0b10101,
                                    0b10101 };                  // �ٴ��� ���� ���ݿ� ���� ���ϰ�
                                    
    unsigned char needle_pat[5][8];                                 // �ٴð� ������ ������������ ���ϰ�
    
    unsigned char addr;                                             // CGRAM�� �ּ� ������ ���� �ӽ� �� ���� ����
                        
    for(i = 0; i<8; i++){                                           
        LCD_WriteCommand(0x40 + i);
        LCD_WriteData(scale_pat[i]);                                // CGRAM�� 0x00�������� �ٴ��� ���� ���ݿ� ���� ���ϰ� ����
    }
        
    addr = 0x47;                                                    // 0x47���� ������ ���ƴ�.
    
    for(j = 0; j < 5; j++) {
        for(i = 0; i < 8; i++){
            needle_pat[j][i] = scale_pat[i] | (0b10000 >> j);       // �ٴ��� ���Ե� ���ݿ� ���� ���ϰ����� ����
            LCD_WriteCommand(++addr);                               // CGRAM�� 0x48�������� �ٴ��� ���Ե� ���ݿ� ���� ���ϰ����� ����
            LCD_WriteData(needle_pat[j][i]);                        // 
        }
    }

    // LCD�� HOME���� 16���� �ٴ��� ���� ������ �׷� ������.
    LCD_WriteCommand(HOME);                                         
    for(i = 0; i < 16; i++){
        LCD_WriteData(0x00);
    }
    
    lcd_index_old = 0;
    needle_index_old = 0;                                           // LCD_UpdateScale()���� ����� ���� �ʱ�ȭ
}

// LCD�� frequency�� �ش�� ���ݿ� �ٴ��� ���Ե� ������ ����ϴ� �����ƾ�̴�.
// �׷��� ������ ���� �ٴ��� ��ġ�� ������Ʈ �ϴ� ���̴�.
void LCD_UpdateScale(float frequency) 
{
    unsigned char lcd_index;                                        // LCD 16���� ������ �ϳ��� ����
    unsigned char needle_index;                                     // LCD ��ĭ �ȿ��� 5���� �� �߿� �ϳ��� ����
    
    // 875 ~ 1080�� 0 ~ 16���� ����ȭ
    lcd_index = 16 * (frequency - 875) / (1080 - 875);  
    if(lcd_index == 16){
        lcd_index = 15;                                             // ���ļ��� 1080�� �� �ٴ��� ������ ����°��� ����
    }
    
    // lcd�� �ε����� ���� �� ������ �ε������� �ٴ��� ���� ������ ����Ѵ�.
    if(lcd_index_old != lcd_index){                                  
        LCD_WriteCommand(HOME);
        LCD_MoveCursor(lcd_index_old);
        LCD_WriteData(0x00);
    }

    // 0 ~ 1�� ���� 1 ~ 6���� ����ȭ (�ٴ��� ��ġ�� �����ϱ� ����)
    needle_index = 5 * ( 16 * (frequency - 875) / (1080 - 875) - lcd_index) + 1; 
    if(needle_index == 6){                                          // ���ļ��� 1080�� �� �ٴ��� ������ ����°��� ����
        needle_index = 5;
    }

    // �ٴ� �ε��� ���� lcd�ε��� ���� �ϳ��� ���� �Ǿ��ٸ� lcd�ε����� �̵��Ͽ� ����� �ٴ��� ������ ������ش�.
    if((needle_index_old != needle_index) || (lcd_index_old != lcd_index)){
        LCD_WriteCommand(HOME);
        LCD_MoveCursor(lcd_index);
        LCD_WriteData(needle_index);
    }
    
    // lcd�ε����� �ٴ� �ε����� old���� ����
    lcd_index_old = lcd_index;
    needle_index_old = needle_index;
    
    // ���� ���� �Ŀ� ���������� ���ݿ� �ش�� ���ļ����� LCD�� ����Ѵ�.
    LCD_WriteCommand(LINE2);
    LCD_ShowFreq(frequency);
}

// LCD�� ��Ʈ���� ����� ���ڿ��� ������ִ� �����ƾ�̴�.
void LCD_PrintString(char *pStr)
{
    // null���ڸ� ������ ������ ��Ʈ���� ����� ���ڸ� ����ϰ� �ȴ�.
    while(*pStr) LCD_WriteData(*pStr++);
}

// LCD�� �ι�° ���ο� ��ȣ�� ���¸� ������ִ� �����ƾ
void LCD_UpdateSignalStatus(unsigned int signalLVL) {
    char str[10];   

    LCD_WriteCommand(LINE2);
    LCD_MoveCursor(12);
	// signalLVL���� 0b0000���� 0b1111������ ���� �Էµǰ� ������� ���� ����Ͽ� ����Ѵ�.
    sprintf(str, "%2d%%", (int)((float)signalLVL / 15 * 99.9));
    LCD_PrintString(str);
}


// LCD�� �ι�° ������ 16ĭ��ŭ Ŭ�������ִ� �����ƾ
void LCD_ClearLine2(void)
{
    unsigned char i;
    LCD_WriteCommand(LINE2);
    for(i = 0; i<16; i++){
        LCD_WriteData(' ');
    }
}