#include <txtlcd.h>

unsigned char lcd_index_old;                                    // LCD인덱스의 이전값을 저장한다.
unsigned char needle_index_old;                                 // 바늘인덱스의 이전값을 저장한다.


// LCD 초기화 함수
void LCD_init(void)
{
    DDRG = 0b00000111;                                          // PG0부터 PG2까지 출력 설정
    DDRA = 0b11111111;                                          // PD0부터 PD7까지 출력 설정
    PORTG &= 0b11111110;                                        // E = 0
            
    delay_ms(15);                                               // Wait for more than 15 ms
    LCD_WriteCommand(0b00110000);                               // Function set
    delay_ms(5);                                                // Wait for more than 4.1 ms
    LCD_WriteCommand(0b00110000);                               // Function set
    delay_us(100);                                              // Wait for more than 100 μs
    LCD_WriteCommand(0b00110000);                               // Function set
            
    LCD_WriteCommand(FUNCSET);                                  // 펑션 셋
    LCD_WriteCommand(DISPON);                                   // 디스플레이 ON
    LCD_WriteCommand(ALLCLR);                                   // LCD클리어
    LCD_WriteCommand(ENTMODE);                                  // 엔트리 모드 셋
}       

// LCD에 인스트럭션을 쓰는 함수
void LCD_WriteCommand(unsigned char byte)       
{       
    LCD_Busy();                                                 // 2ms만큼 대기         
            
    PORTA = byte;                                               // 데이터핀으로 입력할 인스트럭션 출력
    PORTG &= 0b11111001;                                        // RS = 0 
                                                                // RW = 0
    delay_us(1);                                                // t_su의 최소값: 100ns
    PORTG |= 0b00000001;                                        // E = 1
    delay_us(1);                                                // t_w의 최소값: 300ns
    PORTG &= 0b11111110;                                        // E = 0
} 

// LCD에 데이터를 쓰는 함수
void LCD_WriteData(unsigned char byte)
{
    LCD_Busy();                                                 // 2ms만큼 대기     
    
    PORTA = byte;                                               // 데이터핀으로 입력할 데이터 출력
    PORTG |= 0b00000100;                                        // RS = 1
    PORTG &= 0b11111101;                                        // RW = 0
    delay_us(1);                                                // t_su의 최소값: 100ns
    PORTG |= 0b00000001;                                        // E = 1
    delay_us(1);                                                // t_w의 최소값: 300ns
    PORTG &= 0b11111110;                                        // E = 0
}

// LCD에 주파수를 출력해주는 서브루틴 
// 추가로 안테나 모양의 기호도 출력한다.
void LCD_ShowFreq(unsigned int freq)                        
{
    char str[10];                                               // 스트링 배열을 만들고
    LCD_WriteCommand(LINE2);                                    // 두번째 줄로 이동한 다음에
    sprintf(str, " %3d.%dMhz  ", freq / 10, freq % 10 );        // 스트링에 출력할 문자를 저장해주고
    LCD_PrintString(str);                                       // LCD에 출력한다 저장된 문자를
    LCD_WriteData(0b10110111);
}          



// LCD에서 num만큼의 커서를 이동하는 서브루틴
void LCD_MoveCursor(unsigned char num)
{
    unsigned char i;
    for(i = 0; i<num; i++){                                     // num만큼 반복
        LCD_WriteCommand(0b00010100);                           // 커서를 한칸 이동하는 인스트럭션
    }
}

// 비지플래그를 체크하는대신 2ms대기하는 서브루틴
void LCD_Busy(void)
{
    delay_ms(2);
}

// LCD에 라디오 눈금을 출력해주는 서브루틴
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
                                    0b10101 };                  // 바늘이 없는 눈금에 대한 패턴값
                                    
    unsigned char needle_pat[5][8];                                 // 바늘과 눈금이 겹쳐있을때의 패턴값
    
    unsigned char addr;                                             // CGRAM의 주소 번지에 대한 임시 값 저장 변수
                        
    for(i = 0; i<8; i++){                                           
        LCD_WriteCommand(0x40 + i);
        LCD_WriteData(scale_pat[i]);                                // CGRAM의 0x00번지부터 바늘이 없는 눈금에 대한 패턴값 저장
    }
        
    addr = 0x47;                                                    // 0x47까지 저장을 마쳤다.
    
    for(j = 0; j < 5; j++) {
        for(i = 0; i < 8; i++){
            needle_pat[j][i] = scale_pat[i] | (0b10000 >> j);       // 바늘이 포함된 눈금에 대한 패턴값들을 생성
            LCD_WriteCommand(++addr);                               // CGRAM의 0x48번지부터 바늘이 포함된 눈금에 대한 패턴값들을 저장
            LCD_WriteData(needle_pat[j][i]);                        // 
        }
    }

    // LCD의 HOME부터 16개의 바늘이 없는 눈금을 그려 나간다.
    LCD_WriteCommand(HOME);                                         
    for(i = 0; i < 16; i++){
        LCD_WriteData(0x00);
    }
    
    lcd_index_old = 0;
    needle_index_old = 0;                                           // LCD_UpdateScale()에서 사용할 변수 초기화
}

// LCD에 frequency에 해당된 눈금에 바늘이 포함된 패턴을 출력하는 서브루틴이다.
// 그려진 스케일 위에 바늘의 위치를 업데이트 하는 것이다.
void LCD_UpdateScale(float frequency) 
{
    unsigned char lcd_index;                                        // LCD 16개의 공간중 하나를 선택
    unsigned char needle_index;                                     // LCD 한칸 안에서 5개의 줄 중에 하나를 선택
    
    // 875 ~ 1080을 0 ~ 16으로 정규화
    lcd_index = 16 * (frequency - 875) / (1080 - 875);  
    if(lcd_index == 16){
        lcd_index = 15;                                             // 주파수가 1080일 때 바늘이 눈금을 벗어나는것을 방지
    }
    
    // lcd의 인덱스가 변할 때 이전의 인덱스에는 바늘이 없는 패턴을 출력한다.
    if(lcd_index_old != lcd_index){                                  
        LCD_WriteCommand(HOME);
        LCD_MoveCursor(lcd_index_old);
        LCD_WriteData(0x00);
    }

    // 0 ~ 1의 값을 1 ~ 6으로 정규화 (바늘의 위치를 결정하기 위함)
    needle_index = 5 * ( 16 * (frequency - 875) / (1080 - 875) - lcd_index) + 1; 
    if(needle_index == 6){                                          // 주파수가 1080일 때 바늘이 눈금을 벗어나는것을 방지
        needle_index = 5;
    }

    // 바늘 인덱스 값과 lcd인덱스 값이 하나라도 변경 되었다면 lcd인덱스로 이동하여 변경된 바늘의 패턴을 출력해준다.
    if((needle_index_old != needle_index) || (lcd_index_old != lcd_index)){
        LCD_WriteCommand(HOME);
        LCD_MoveCursor(lcd_index);
        LCD_WriteData(needle_index);
    }
    
    // lcd인덱스와 바늘 인덱스를 old값에 저장
    lcd_index_old = lcd_index;
    needle_index_old = needle_index;
    
    // 눈금 수정 후에 마지막으로 눈금에 해당된 주파수값을 LCD에 출력한다.
    LCD_WriteCommand(LINE2);
    LCD_ShowFreq(frequency);
}

// LCD에 스트링에 저장된 문자열을 출력해주는 서브루틴이다.
void LCD_PrintString(char *pStr)
{
    // null문자를 만나기 전까지 스트링에 저장된 문자를 출력하게 된다.
    while(*pStr) LCD_WriteData(*pStr++);
}

// LCD의 두번째 라인에 신호의 상태를 출력해주는 서브루틴
void LCD_UpdateSignalStatus(unsigned int signalLVL) {
    char str[10];   

    LCD_WriteCommand(LINE2);
    LCD_MoveCursor(12);
	// signalLVL에는 0b0000에서 0b1111까지의 값이 입력되고 백분율의 값을 계산하여 출력한다.
    sprintf(str, "%2d%%", (int)((float)signalLVL / 15 * 99.9));
    LCD_PrintString(str);
}


// LCD의 두번째 라인을 16칸만큼 클리어해주는 서브루틴
void LCD_ClearLine2(void)
{
    unsigned char i;
    LCD_WriteCommand(LINE2);
    for(i = 0; i<16; i++){
        LCD_WriteData(' ');
    }
}