#include <mega128.h>
#include <delay.h>
#include <txtlcd.h>

#define SLA_W           0b11000000                              // TEA5767의 쓰기 주소
#define SLA_R           0b11000001                              // TEA5767의 읽기 주소

#define MIN_FREQ        875
#define MAX_FREQ        1080

void TEA5767_GetStatus(void);
void TEA5767_Init(void);
void TEA5767_SetFreq(unsigned int);
void TEA5767_WriteData(void);
void TEA5767_ReadData(void);

struct TEA5767_status
{
    /* TEA5767_GetStatus() 함수가 실행되면 TEA5767으로부터 다음과 같은 정보들을 읽어온다.*/
    unsigned int currentFreq;                                   // 설정된 주파수 값
    unsigned char signalLVL;                                    // FM신호 세기
    unsigned char isReady;                                      // ReadyFlag
    unsigned char isBandlimitReached;                           // BandLimitFlag
    unsigned char isStereoSignal;                               // 설정된 채널의 스테레오 여부
};

struct TEA5767_status status;                                   // 구초체를 생성

unsigned int preset_frequency[4] = {0, 0, 0, 0};                // 주파수 프리셋이 저장될 배열
unsigned char presetCount = 0;                                  // 프리셋이 저장될 순서를 저장하는 변수
unsigned char presetNum = 0;                                    // 로드될 프리셋의 번호를 저장하는 변수
unsigned char maxPresetNum = 0;                                 // 저장된 프리셋의 갯수가 저장될 변수
unsigned char isPresetLoaded = 0;                               // 프리셋 로드가 완료 되었음을 나타내는 변수

unsigned char muteOn = 0;                                       // 뮤트의 ON/OFF 여부를 저장하는 변수
unsigned char searchModeOn = 0;                                 // 서치모드 여부를 저장하는 변수
unsigned char forcedMonoModOn = 0;                              // 강제 모노출력 모드 여부를 저장하는 변수
unsigned char isSearchingUp = 0;                                // 서치모드의 탐색 방향이 위쪽인지 여부를 저장하는 변수
unsigned char isLoadingPreset = 0;                              // 현재 프리셋모드로 주파수가 설정되고 있는지 여부를 저장하는 변수

unsigned char write_bytes[5] = { 0x00,0x00,0x00,0x00,0x00 };    // ATMega128로부터 송신될 데이터가 저장되는 변수
unsigned char read_bytes[5]  = { 0x00,0x00,0x00,0x00,0x00 };    // TEA5767로부터 수신되는 데이터가 저장되는 변수

unsigned int frequency = MIN_FREQ;                              // 주파수값을 저장하는 변수
unsigned int frequency_old = MIN_FREQ;                          // 이전의 주파수값을 저장하는 변수
unsigned int i = 0;                                             // 인터럽트 반복문을 위한 변수
char str_buf[17];                                               // LCD에 출력될 문자를 저장하는 배열

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MAIN~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void main(void)
{
    DDRE = 0x00;                                                // PINE.3을 입력으로 설정(로터리 인코더 데이터 입력)
    PORTD = 0xFF;                                               // PORTD에 연결된 스위치 외부풀업 사용(EXT_INT2, EXT_INT3) 
    DDRD = 0xFF;                                                // EXT_INT 2(PD2), 3(PD3), TWI통신핀(PD1, PD0) 초기 설정
    DDRB = 0xFF;                                                // PB0(mute)과 PB1(mono)핀을 출력으로 설정(두개의 상태표시 LED)
    PORTB = 0xFF;                                               // 초기 LED모두 OFF
    
    EIMSK = 0b11111100;                                         // EXT_INT 2, 3, 4, 5, 6, 7 Enable
    EICRA = 0b10100000;                                         // EXT_INT 2, 3 Falling Edge Detection
    EICRB = 0b10101010;                                         // EXT_INT 4, 5, 6, 7 Falling Edge Detection
    
    LCD_init();                                                 // LCD 초기화
    LCD_DrawScale();                                            // LCD에 라디오 눈금 출력
    TEA5767_Init();                                             // TEA5767 초기화
    SREG = 0x80;                                                // Global Interrupt Enable

    while (1);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~TEA5657~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// TEA5767에 입력할 초기 데이터들을 설정하는 함수
void TEA5767_Init(void)
{
    TWBR = 0x0C;                                                // 16MHz / (16 + 2 * TWBR * 4^TWPS) = 400KHz
    TWSR = (0 << TWPS1) | (0 << TWPS0);                         // 400KHz로 설정했기 때문에 분주비 사용x
    
                                                                // TEA5767의 초기값을 다음과 같이 세팅
    write_bytes[0] = 0b00000000;                                // UnMute / NotSearchMode / PLL[13:8]
    write_bytes[1] = 0b00000000;                                // PLL[7:0]
    write_bytes[2] = 0b01110000;                                // SearchDownMode / SearchStopLevel: High / HighSideInjection / Stereo / UnMuteRL
    write_bytes[3] = 0b00011110;                                // NotStandbyMode / Europe FM Band / ClockFreq 32.768 kHz / SoftMuteOn / 
                                                                // HighCutControlOn / StereoNoiseCancellingOn
    write_bytes[4] = 0b01000000;                                // PLLREF 32.768 kHz / de-emphasis 75ms 

    TEA5767_SetFreq(frequency);                                 // 주파수 초기값 설정
    TEA5767_WriteData();                                        // ATMega128 -> TEA5767 5바이트 데이터 입력
    delay_ms(1000);                                             // 충분한 시간을 기다린 후에
    TEA5767_GetStatus();                                        // Read initial status
    LCD_UpdateSignalStatus(status.signalLVL);                   // 신호의 세기를 LCD에 출력
}

// TEA5767에 입력될 주파수값을 설정해주는 함수
void TEA5767_SetFreq(unsigned int value)
{
    unsigned int PLL;                                           // save PLL[14] to 16bit unsigned int

    if(value > MAX_FREQ)
        value = MIN_FREQ;                                       // 주파수 최대값 설정
    if(value < MIN_FREQ)    
        value = MAX_FREQ;                                       // 주파수 최대값 설정
    
    // Reference Frequency = 32768 Hz
    // Calculate PLL with high side injection formula
    PLL = 4 * ((float)value * 100000 + 225000) / 32768 + 0.5;

    write_bytes[0] = (write_bytes[0] & 0xC0) | (PLL >> 8);      // 계산된 PLL값을 입력될 데이터에 저장
    write_bytes[1] = PLL & 0xFF;
}

// TEA5767의 상태 데이터들을 가져오는 함수
void TEA5767_GetStatus(void)
{
    TEA5767_ReadData();                                         // Read data from TEA5767
    
    // 받아온 PLL값을 세자리 정수의 주파수 값으로 변환
    status.currentFreq = (((float)(((unsigned int)(read_bytes[0] & 0x3F) << 8) + read_bytes[1]) * 32768) / 4 - 225000) / 100000 + 0.5;
    
    // if RF = 1 then a station has been found or the band limit has been reached; if RF = 0 then no station has been found
    status.isReady = (read_bytes[0] & 0x80 != 0) ? 1 : 0;
    
    //if BLF = 1 then the band limit has been reached; if BLF = 0 then the band limit has not been reached
    status.isBandlimitReached = (read_bytes[0] & 0x40 != 0) ? 1 : 0;
    
    //if STEREO = 1 then stereo reception; if STEREO = 0 then mono reception
    status.isStereoSignal = (read_bytes[2] & 0x80 != 0) ? 1 : 0;
    
    // level AVC output
    status.signalLVL = (read_bytes[3] & 0xF0) >> 4;
}   


// TEA5767에 5바이트의 데이터를 전송해주는 함수
void TEA5767_WriteData()
{
    unsigned char i;
    
    // 수동 주파수 조절 모드 프리셋 또는 주파수 설정 도중이라면 설정될 주파수 값을 바로 LCD눈금에 동기화
    if(searchModeOn == 0 || isLoadingPreset != 0){
        frequency = (((float)(((unsigned int)(write_bytes[0] & 0x3F) << 8) + write_bytes[1]) * 32768) / 4 - 225000) / 100000 + 0.5;
        LCD_UpdateScale(frequency);
    }
    
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);               // clear TWINT / start I2C / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // TWINT가 1이 되어 I2C통신이 준비될 때까지 대기
    
    
    TWDR = SLA_W;                                                   // TEA5767의 쓰기 주소를 저장
    TWCR = (1 << TWINT) | (1 << TWEN);                              // clear TWINT / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // 통신이 끝나고 TWINT가 set되기 전까지 대기
    
    
    // ATMega128 -> TEA5767 5바이트의 데이터를 전송
    for (i = 0; i < 5; i++) 
    {   
        TWDR = write_bytes[i];                                      // 전송할 데이터 TWDR레지스터에 입력
        TWCR = (1 << TWINT) | (1 << TWEN);                          // clear TWINT / enable I2C
        while (!(TWCR & (1 << TWINT)));                             // 통신이 끝나고 TWINT가 set되기 전까지 대기
    }   
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);               // TWINT를 확인함으로서 데이터 전송이 잘 이루어졌나 확인, I2C stop
    while (TWCR & (1 << TWSTO));                                    // wait until stop condition is executed
        
    isLoadingPreset = 0;                                            // 프리셋 주파수 설정이 완료되었음을 나타낸다.
}   
    
// TEA5767로부터 5바이트의 데이터를 읽어오는 함수    
void TEA5767_ReadData() 
{   
    unsigned char i;
        
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);               // clear TWINT / start I2C / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // TWINT가 1이 되어 I2C통신이 준비될 때까지 대기
    
    
    TWDR = SLA_R;                                                   // TEA5767의 읽기 주소를 저장
    TWCR = (1 << TWINT) | (1 << TWEN);                              // clear TWINT / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // 통신이 끝나고 TWINT가 set되기 전까지 대기 


    // TEA5767 -> ATMega128 5바이트의 데이터를 전송
    for (i = 0; i < 5; i++)
    {
        if (i != 4)                                                 // 마지막 데이터가 아니라면
            TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA);                    // TWEA비트를 set해서 ACK신호를 출력
        else
            TWCR=(1<<TWINT)|(1<<TWEN);                              // 마지막 데이터를 수신 받았을 때 NACK 신호를 출력
        while (!(TWCR & (1 << TWINT)));                             // 통신이 끝나고 TWINT가 set되기 전까지 대기 
        read_bytes[i] = TWDR;                                       // 수신한 데이터 저장
    }
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);               // TWINT를 확인함으로서 데이터 전송이 잘 이루어졌나 확인, I2C stop
    while (TWCR & (1 << TWSTO));                                    // wait until stop condition is executed
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Interrupt~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// 프리셋 세이브 버튼이 눌렸을 때 발생하는 인터럽트 루틴 
// 이 스위치가 눌리면 현재의 주파수를 프리셋1부터 프리셋4까지 총 네개까지 저장하게 된다. 저장하려는 주파수가 이미 프리셋에 저장되어 있다면 저장하지 않은다.
// 새로운 주파수값이라면 이전에 저장된 프리셋의 다음 배열부터 프리셋을 하나씩 추가한다. 프리셋4까지 저장되었다면 다음에는 프리셋1부터 다시 저장하게 된다. 
interrupt[EXT_INT2] void SavePresetSwitchOn(void)
{
    i = 0;                                                      
    while(1){                                                       
        if(preset_frequency[i] == frequency){                       // preset_frequency[0]부터 [3]까지 검사를 해서 일치하는 주파수가 존재한다면  
            LCD_ClearLine2();                                                       
            LCD_WriteCommand(LINE2);                                // already saved! 라는 문구를 LCD에 출력한다.
            LCD_PrintString("already saved!");                      
            break;
        }
        if(i > 2){                                                  // 일치하는 주파수가 없다면
            if(presetCount > 3){
                presetCount = 0;                                    // 프리셋4까지 저장되었다면 이젠 프리셋1을 저장한다.
            }

            preset_frequency[presetCount] = frequency;              // presetCount에 해당하는 인덱스에 현재 주파수를 저장한다.

            if(presetCount + 1 > maxPresetNum){
                maxPresetNum = presetCount + 1;                     // 프리셋에 현재 주파수를 저장하고 저장된 프리셋의 갯수를 계산한다.
            }
            
            LCD_ClearLine2();
            LCD_WriteCommand(LINE2);
            sprintf(str_buf, "PRESET%d saved!", ++presetCount);     // 저장된 프리셋 번호를 LCD에 출력하고 presetCount값을 1 증가시킨다.
            LCD_PrintString(str_buf);
            break;
        }
        i++;
    }
    delay_ms(2000);                                                 // 2초 후에는 현재 주파수와 신호 세기 다시 출력               
    LCD_ClearLine2();                                               
    LCD_ShowFreq(frequency);                                        
    LCD_UpdateSignalStatus(status.signalLVL);                       
    EIFR = 0xFF;                                                    // 딜레이 동안에 발생한 인터럽트 플래그를 초기화
}


// 프리셋을 로드 스위치를 눌렀을 때 발생하는 인터럽트 루틴
// 저장된 프리셋이 하나도 없다면 프리셋을 로드하지 않는다.
// 스위치가 눌리면 LCD에 프리셋 선택 문구가 출력된다. 버튼을 누를 때마다 프리셋은 1부터 저장된 갯수 만큼 까지 선택되고
// 프리셋을 선택한 후에 2초간 대기하면 선택한 프리셋으로 주파수가 설정되게 된다. 
interrupt[EXT_INT3] void LoadPresetSwitchOn(void)
{
    if(maxPresetNum == 0){                                          // 프리셋을 저장한 적이 없다면
        LCD_ClearLine2();
        LCD_WriteCommand(LINE2);                                    // "no preset saved"라는 문구를 2초간 띄우고
        LCD_PrintString("no preset saved");
        delay_ms(2000);
        LCD_ClearLine2();
        LCD_ShowFreq(frequency);                                    // 2초 후에는 다시 주파수와 신호 레벨을 출력한다.
        LCD_UpdateSignalStatus(status.signalLVL);
    }
    
    else{                                                           // 프리셋을 저장한 적이 있다면
        isPresetLoaded = 0;                                         
        presetNum++;                                                // 프리셋번호를 1로 설정한다.
        if(presetNum > maxPresetNum){                               
            presetNum = 1;                                          // 프리셋번호가 max값을 초과했을 때 다시 1로 설정한다.
        }
        
        LCD_ClearLine2();
        LCD_WriteCommand(LINE2);
        sprintf(str_buf, "selected:PRESET%d", presetNum);           // 선택된 프리셋 값을 LCD에 출력한다.
        LCD_PrintString(str_buf);
        
        EIMSK = 0b00001000;                                         // 외부인터럽트 3만 개별 활성화 시키고
        SREG = 0x80;                                                // 전역 인터럽트를 잠시 활성화 시킨 후에 2초간기다린다.
        delay_ms(2000);                                             // 2초 동안에 이 버튼이 다시 눌린다면 다시 인터럽트가 발생하여
                                                                    // 프리셋 2, 3, 4 까지 선택할 수 있게 된다.
        
        if(isPresetLoaded == 0){
            isLoadingPreset = 1;                                    // 프리셋을 통한 주파수 선택 여부 true
            isPresetLoaded = 1;                                     // 프리셋로드를 하게 되었는지 여부 true(인터럽트 중첩 방지)
            SREG = 0x00;                                            // 전역 인터럽트를 비활성화 하고
            EIMSK = 0b11111100;                                     // EXT_INT 2, 3, 4, 5, 6, 7 Enable
            TEA5767_SetFreq(preset_frequency[presetNum-1]);         // 
            presetNum = 0;                                          // 프리셋 넘버를 다시 0으로 초기화
            TEA5767_WriteData();                                    // 선택된 프리셋에 해당하는 주파수로 튜닝
            LCD_ClearLine2();                                       //
            LCD_ShowFreq(frequency);                                // 설정된 채널의 주파수값을 출력하고
            delay_ms(1000);                                         // 충분한 시간을 기다리고 설정된 채널에 대한 
            TEA5767_GetStatus();                                    // 통신 상태를 읽어온다
            LCD_UpdateSignalStatus(status.signalLVL);               // 그리고 통신 상태를 출력한다.
        }
    }
    EIFR = 0xFF;                                                    // 도중에 발생한 인터럽트를 모두 초기화 시킨다.
}


// RotaryEncoder가 회전하면 발생하는 인터럽트 루틴
// 서칭모드: TEA5767에서 설정된 서칭 방향에 따라서 채널을 탐색하게 되는데 출력신호 레벨이 일정 레벨보다 높을 때 그 채널로 튜닝하는 모드.
//        현재는 출력신호 레벨이 High(십진값으로 10)을 넘는 주파수를 탐색하도록 TEA5767_Init()함수에서 초기화 해주었음
// RotaryEncoder의 노브를 돌리면 회전 방향에 따라서 주파수를 증가시키거나 감소시키면서 라디오를 튜닝한다.
// 서칭모드가 활성화 되어 있는 상태라면 회전 방향에 따라서 서칭을 시작한다.
interrupt[EXT_INT4] void RotaryEncode(void)
{
    frequency_old = frequency;                                      // 튜닝 이전의 주파수를 가져온다.
                
    if (PINE.3){                                                    // PINE.3에 입력된 값이 1이라면 오른쪽으로 회전하는것이다.
        write_bytes[2] |= 0x80;                                     // 서치모드의 방향을 위쪽으로 설정한다(서칭모드일때만 유의미).
        TEA5767_SetFreq( frequency + 1 );                           // 현재 주파수보다 0.1Mhz높은 주파수로 세팅한다.
        isSearchingUp = 1;                                          // 주파수가 증가하는 방향으로 서칭여부 yes(서칭모드일때만 유의미)
    }
    else {                                                          // PINE.3에 입력된 값이 0이라면 왼쪽으로 화전하는것이다.
        write_bytes[2] &= ~0x80;                                    // 서치모드의 방향을 아래쪽으로 설정한다(서칭모드일때만 유의미).
        TEA5767_SetFreq( frequency - 1 );                           // 현재 주파수보다 0.1Mhz낮은 주파수로 세팅한다.
        isSearchingUp = 0;                                          // 주파수가 증가하는 방향으로 서칭여부 no(서칭모드일때만 유의미)
    }           
    TEA5767_WriteData();                                            // Write data to TEA5767
                
    if(searchModeOn != 0){                                          // 서치모드라면 
        delay_ms(500);                                              // 서칭 완료될 때까지 충분한 시간을 기다리고
        TEA5767_GetStatus();                                        // 찾아낸 채널의 주파수를 가져온다.
        frequency = status.currentFreq;                             // 그 주파수를 현재 주파수 변수에 저장한다.
        // 주파수 서칭 모드에 대한 LCD 애니메이션을 구현하기 위한 코드
        if(isSearchingUp == 1){                                     // 찾는 방향이 주파수가 증가하는 방향이면
            for( i = frequency_old; i != frequency + 1; i++){       // 서칭 모드 이전의 주파수부터 서칭 완료된 주파수 까지 i를 증가시키면서
                if(i > 1080){                                       // LCD에 출력된 눈금의 바늘을 움직이게 한다.
                    i = 875;                                        // 1080를 초과하게 되면 875부터 i가 증가하도록 한다.
                }
                delay_ms(10);
                LCD_UpdateScale(i);                                 // i값에 따라 바늘을 움직이게 하는 함수
            }
        }
        else{                                                       // 찾는 방향이 주파수가 감소하는 방향이라면
            for( i = frequency_old; i != frequency - 1; i--){       // 서칭 모드 이전의 주파수부터 서칭 완료된 주파수 까지 i를 증가시키면서
                if(i < 875){                                        // LCD에 출력된 눈금의 바늘을 움직이게 한다.
                    i = 1080;                                       // 1080를 초과하게 되면 875부터 i가 증가하도록 한다.
                }                                                   
                delay_ms(10);                                       
                LCD_UpdateScale(i);                                 // i값에 따라 바늘을 움직이게 하는 함수
            }
        }
        delay_ms(500);                                              // 500ms이상의 충분한 시간 후에
        TEA5767_GetStatus();                                        // 현재 채널의 신호 세기를 받아와서
        LCD_UpdateSignalStatus(status.signalLVL);                   // LCD에 세기를 나타낸다.
    }                                                                                                           
    
    else{                                                           // 서치모드가 아니라면
        delay_ms(100);                                              // 빠른 채널 선택을 위해 100ms만 기다리고
        TEA5767_GetStatus();                                        // 현재 채널의 신호 세기를 받아와서
        LCD_UpdateSignalStatus(status.signalLVL);                   // LCD에 세기를 나타낸다.
    }
    EIFR = 0xFF;                                                    // 정확한 회전 방향을 읽어오기 위해서 인터럽트 중첩은 허용하지 않는다.
}           

// RotaryEncoder에 내장된 택트 스위치가 눌리면 발생하는 인터럽트 서비스 루틴
// 서치모드 ON/OFF기능을 담당한다.
interrupt[EXT_INT5] void RotarySwitchOn(void)           
{           
    if(searchModeOn == 0){                                          // 현재 서치모드가 아니었다면                           
        write_bytes[0] |= 0x40;                                     // Search Flag를 set시켜 서치모드 ON
        searchModeOn = 1;                                           // 서치모드 여부 true
    }           
    else{                                                           // 현재 서치모드 였다면
        write_bytes[0] &= ~0x40;                                    // Search Flag를 clear시켜 서치모드 OFF
        searchModeOn = 0;                                           // 서치모드 여부 false
    }
}

// 뮤트 스위치가 눌렸을때 발생하는 인터럽트 서비스 루틴
// 뮤트 ON/OFF기능을 담당한다.
interrupt[EXT_INT6] void MuteSwitchOn(void)
{
    if(muteOn == 0){                                                // 현재 뮤트가 ON이 아니었다면
        write_bytes[0] |= 0x80;                                     // Mute on Flag를 set시켜 뮤트 ON
        muteOn = 1;                                                 // 뮤트 여부 true
        PORTB &= ~0x10;                                             // PB0에 연결된 적색 LED ON
    }
    else{                                                           // 현재 뮤트가 OFF였다면
        write_bytes[0] &= ~0x80;                                    // Mute on Flag를 clear시켜 뮤트 OFF
        muteOn = 0;                                                 // 뮤트 여부 false
        PORTB |= 0x10;                                              // PB0에 연결된 적색 LED OFF
    }
    TEA5767_WriteData();                                            // 뮤트 ON/OFF 설정
}


// 강제 모노 모드 스위치가 눌렸을때 발생하는 인터럽트 서비스 루틴
// 강제 모노 모드 ON/OFF기능을 담당한다.
interrupt[EXT_INT7] void ForcedMonoSwitchOn(void)
{
    if(forcedMonoModOn == 0){                                       // 현재 강제 모노 모드 ON이 아니었다면
        write_bytes[2] |= 0x08;                                     // Forced Mono Flag를 set시켜 강제 모노 ON
        forcedMonoModOn = 1;                                        // 강제 모노 출력 모드 여부 true
        PORTB &= ~0x20;                                             // PB0에 연결된 적색 LED ON
    }                                                               
    else{                                                           // 현재 뮤트가 OFF였다면
        write_bytes[2] &= ~0x08;                                    // Forced Mono Flag를 clear시켜 강제 모노OFF
        forcedMonoModOn = 0;                                        // 강제 모노 출력 모드 여부 false
        PORTB |= 0x20;                                              // PB0에 연결된 적색 LED OFF
    }                                                               
    TEA5767_WriteData();                                            // 강제 모노 출력 모드 ON/OFF 설정
}