#include <mega128.h>
#include <delay.h>
#include <txtlcd.h>

#define SLA_W           0b11000000                              // TEA5767�� ���� �ּ�
#define SLA_R           0b11000001                              // TEA5767�� �б� �ּ�

#define MIN_FREQ        875
#define MAX_FREQ        1080

void TEA5767_GetStatus(void);
void TEA5767_Init(void);
void TEA5767_SetFreq(unsigned int);
void TEA5767_WriteData(void);
void TEA5767_ReadData(void);

struct TEA5767_status
{
    /* TEA5767_GetStatus() �Լ��� ����Ǹ� TEA5767���κ��� ������ ���� �������� �о�´�.*/
    unsigned int currentFreq;                                   // ������ ���ļ� ��
    unsigned char signalLVL;                                    // FM��ȣ ����
    unsigned char isReady;                                      // ReadyFlag
    unsigned char isBandlimitReached;                           // BandLimitFlag
    unsigned char isStereoSignal;                               // ������ ä���� ���׷��� ����
};

struct TEA5767_status status;                                   // ����ü�� ����

unsigned int preset_frequency[4] = {0, 0, 0, 0};                // ���ļ� �������� ����� �迭
unsigned char presetCount = 0;                                  // �������� ����� ������ �����ϴ� ����
unsigned char presetNum = 0;                                    // �ε�� �������� ��ȣ�� �����ϴ� ����
unsigned char maxPresetNum = 0;                                 // ����� �������� ������ ����� ����
unsigned char isPresetLoaded = 0;                               // ������ �ε尡 �Ϸ� �Ǿ����� ��Ÿ���� ����

unsigned char muteOn = 0;                                       // ��Ʈ�� ON/OFF ���θ� �����ϴ� ����
unsigned char searchModeOn = 0;                                 // ��ġ��� ���θ� �����ϴ� ����
unsigned char forcedMonoModOn = 0;                              // ���� ������ ��� ���θ� �����ϴ� ����
unsigned char isSearchingUp = 0;                                // ��ġ����� Ž�� ������ �������� ���θ� �����ϴ� ����
unsigned char isLoadingPreset = 0;                              // ���� �����¸��� ���ļ��� �����ǰ� �ִ��� ���θ� �����ϴ� ����

unsigned char write_bytes[5] = { 0x00,0x00,0x00,0x00,0x00 };    // ATMega128�κ��� �۽ŵ� �����Ͱ� ����Ǵ� ����
unsigned char read_bytes[5]  = { 0x00,0x00,0x00,0x00,0x00 };    // TEA5767�κ��� ���ŵǴ� �����Ͱ� ����Ǵ� ����

unsigned int frequency = MIN_FREQ;                              // ���ļ����� �����ϴ� ����
unsigned int frequency_old = MIN_FREQ;                          // ������ ���ļ����� �����ϴ� ����
unsigned int i = 0;                                             // ���ͷ�Ʈ �ݺ����� ���� ����
char str_buf[17];                                               // LCD�� ��µ� ���ڸ� �����ϴ� �迭

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MAIN~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void main(void)
{
    DDRE = 0x00;                                                // PINE.3�� �Է����� ����(���͸� ���ڴ� ������ �Է�)
    PORTD = 0xFF;                                               // PORTD�� ����� ����ġ �ܺ�Ǯ�� ���(EXT_INT2, EXT_INT3) 
    DDRD = 0xFF;                                                // EXT_INT 2(PD2), 3(PD3), TWI�����(PD1, PD0) �ʱ� ����
    DDRB = 0xFF;                                                // PB0(mute)�� PB1(mono)���� ������� ����(�ΰ��� ����ǥ�� LED)
    PORTB = 0xFF;                                               // �ʱ� LED��� OFF
    
    EIMSK = 0b11111100;                                         // EXT_INT 2, 3, 4, 5, 6, 7 Enable
    EICRA = 0b10100000;                                         // EXT_INT 2, 3 Falling Edge Detection
    EICRB = 0b10101010;                                         // EXT_INT 4, 5, 6, 7 Falling Edge Detection
    
    LCD_init();                                                 // LCD �ʱ�ȭ
    LCD_DrawScale();                                            // LCD�� ���� ���� ���
    TEA5767_Init();                                             // TEA5767 �ʱ�ȭ
    SREG = 0x80;                                                // Global Interrupt Enable

    while (1);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~TEA5657~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// TEA5767�� �Է��� �ʱ� �����͵��� �����ϴ� �Լ�
void TEA5767_Init(void)
{
    TWBR = 0x0C;                                                // 16MHz / (16 + 2 * TWBR * 4^TWPS) = 400KHz
    TWSR = (0 << TWPS1) | (0 << TWPS0);                         // 400KHz�� �����߱� ������ ���ֺ� ���x
    
                                                                // TEA5767�� �ʱⰪ�� ������ ���� ����
    write_bytes[0] = 0b00000000;                                // UnMute / NotSearchMode / PLL[13:8]
    write_bytes[1] = 0b00000000;                                // PLL[7:0]
    write_bytes[2] = 0b01110000;                                // SearchDownMode / SearchStopLevel: High / HighSideInjection / Stereo / UnMuteRL
    write_bytes[3] = 0b00011110;                                // NotStandbyMode / Europe FM Band / ClockFreq 32.768 kHz / SoftMuteOn / 
                                                                // HighCutControlOn / StereoNoiseCancellingOn
    write_bytes[4] = 0b01000000;                                // PLLREF 32.768 kHz / de-emphasis 75ms 

    TEA5767_SetFreq(frequency);                                 // ���ļ� �ʱⰪ ����
    TEA5767_WriteData();                                        // ATMega128 -> TEA5767 5����Ʈ ������ �Է�
    delay_ms(1000);                                             // ����� �ð��� ��ٸ� �Ŀ�
    TEA5767_GetStatus();                                        // Read initial status
    LCD_UpdateSignalStatus(status.signalLVL);                   // ��ȣ�� ���⸦ LCD�� ���
}

// TEA5767�� �Էµ� ���ļ����� �������ִ� �Լ�
void TEA5767_SetFreq(unsigned int value)
{
    unsigned int PLL;                                           // save PLL[14] to 16bit unsigned int

    if(value > MAX_FREQ)
        value = MIN_FREQ;                                       // ���ļ� �ִ밪 ����
    if(value < MIN_FREQ)    
        value = MAX_FREQ;                                       // ���ļ� �ִ밪 ����
    
    // Reference Frequency = 32768 Hz
    // Calculate PLL with high side injection formula
    PLL = 4 * ((float)value * 100000 + 225000) / 32768 + 0.5;

    write_bytes[0] = (write_bytes[0] & 0xC0) | (PLL >> 8);      // ���� PLL���� �Էµ� �����Ϳ� ����
    write_bytes[1] = PLL & 0xFF;
}

// TEA5767�� ���� �����͵��� �������� �Լ�
void TEA5767_GetStatus(void)
{
    TEA5767_ReadData();                                         // Read data from TEA5767
    
    // �޾ƿ� PLL���� ���ڸ� ������ ���ļ� ������ ��ȯ
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


// TEA5767�� 5����Ʈ�� �����͸� �������ִ� �Լ�
void TEA5767_WriteData()
{
    unsigned char i;
    
    // ���� ���ļ� ���� ��� ������ �Ǵ� ���ļ� ���� �����̶�� ������ ���ļ� ���� �ٷ� LCD���ݿ� ����ȭ
    if(searchModeOn == 0 || isLoadingPreset != 0){
        frequency = (((float)(((unsigned int)(write_bytes[0] & 0x3F) << 8) + write_bytes[1]) * 32768) / 4 - 225000) / 100000 + 0.5;
        LCD_UpdateScale(frequency);
    }
    
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);               // clear TWINT / start I2C / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // TWINT�� 1�� �Ǿ� I2C����� �غ�� ������ ���
    
    
    TWDR = SLA_W;                                                   // TEA5767�� ���� �ּҸ� ����
    TWCR = (1 << TWINT) | (1 << TWEN);                              // clear TWINT / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // ����� ������ TWINT�� set�Ǳ� ������ ���
    
    
    // ATMega128 -> TEA5767 5����Ʈ�� �����͸� ����
    for (i = 0; i < 5; i++) 
    {   
        TWDR = write_bytes[i];                                      // ������ ������ TWDR�������Ϳ� �Է�
        TWCR = (1 << TWINT) | (1 << TWEN);                          // clear TWINT / enable I2C
        while (!(TWCR & (1 << TWINT)));                             // ����� ������ TWINT�� set�Ǳ� ������ ���
    }   
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);               // TWINT�� Ȯ�������μ� ������ ������ �� �̷������ Ȯ��, I2C stop
    while (TWCR & (1 << TWSTO));                                    // wait until stop condition is executed
        
    isLoadingPreset = 0;                                            // ������ ���ļ� ������ �Ϸ�Ǿ����� ��Ÿ����.
}   
    
// TEA5767�κ��� 5����Ʈ�� �����͸� �о���� �Լ�    
void TEA5767_ReadData() 
{   
    unsigned char i;
        
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);               // clear TWINT / start I2C / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // TWINT�� 1�� �Ǿ� I2C����� �غ�� ������ ���
    
    
    TWDR = SLA_R;                                                   // TEA5767�� �б� �ּҸ� ����
    TWCR = (1 << TWINT) | (1 << TWEN);                              // clear TWINT / enable I2C
    while (!(TWCR & (1 << TWINT)));                                 // ����� ������ TWINT�� set�Ǳ� ������ ��� 


    // TEA5767 -> ATMega128 5����Ʈ�� �����͸� ����
    for (i = 0; i < 5; i++)
    {
        if (i != 4)                                                 // ������ �����Ͱ� �ƴ϶��
            TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA);                    // TWEA��Ʈ�� set�ؼ� ACK��ȣ�� ���
        else
            TWCR=(1<<TWINT)|(1<<TWEN);                              // ������ �����͸� ���� �޾��� �� NACK ��ȣ�� ���
        while (!(TWCR & (1 << TWINT)));                             // ����� ������ TWINT�� set�Ǳ� ������ ��� 
        read_bytes[i] = TWDR;                                       // ������ ������ ����
    }
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);               // TWINT�� Ȯ�������μ� ������ ������ �� �̷������ Ȯ��, I2C stop
    while (TWCR & (1 << TWSTO));                                    // wait until stop condition is executed
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Interrupt~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// ������ ���̺� ��ư�� ������ �� �߻��ϴ� ���ͷ�Ʈ ��ƾ 
// �� ����ġ�� ������ ������ ���ļ��� ������1���� ������4���� �� �װ����� �����ϰ� �ȴ�. �����Ϸ��� ���ļ��� �̹� �����¿� ����Ǿ� �ִٸ� �������� ������.
// ���ο� ���ļ����̶�� ������ ����� �������� ���� �迭���� �������� �ϳ��� �߰��Ѵ�. ������4���� ����Ǿ��ٸ� �������� ������1���� �ٽ� �����ϰ� �ȴ�. 
interrupt[EXT_INT2] void SavePresetSwitchOn(void)
{
    i = 0;                                                      
    while(1){                                                       
        if(preset_frequency[i] == frequency){                       // preset_frequency[0]���� [3]���� �˻縦 �ؼ� ��ġ�ϴ� ���ļ��� �����Ѵٸ�  
            LCD_ClearLine2();                                                       
            LCD_WriteCommand(LINE2);                                // already saved! ��� ������ LCD�� ����Ѵ�.
            LCD_PrintString("already saved!");                      
            break;
        }
        if(i > 2){                                                  // ��ġ�ϴ� ���ļ��� ���ٸ�
            if(presetCount > 3){
                presetCount = 0;                                    // ������4���� ����Ǿ��ٸ� ���� ������1�� �����Ѵ�.
            }

            preset_frequency[presetCount] = frequency;              // presetCount�� �ش��ϴ� �ε����� ���� ���ļ��� �����Ѵ�.

            if(presetCount + 1 > maxPresetNum){
                maxPresetNum = presetCount + 1;                     // �����¿� ���� ���ļ��� �����ϰ� ����� �������� ������ ����Ѵ�.
            }
            
            LCD_ClearLine2();
            LCD_WriteCommand(LINE2);
            sprintf(str_buf, "PRESET%d saved!", ++presetCount);     // ����� ������ ��ȣ�� LCD�� ����ϰ� presetCount���� 1 ������Ų��.
            LCD_PrintString(str_buf);
            break;
        }
        i++;
    }
    delay_ms(2000);                                                 // 2�� �Ŀ��� ���� ���ļ��� ��ȣ ���� �ٽ� ���               
    LCD_ClearLine2();                                               
    LCD_ShowFreq(frequency);                                        
    LCD_UpdateSignalStatus(status.signalLVL);                       
    EIFR = 0xFF;                                                    // ������ ���ȿ� �߻��� ���ͷ�Ʈ �÷��׸� �ʱ�ȭ
}


// �������� �ε� ����ġ�� ������ �� �߻��ϴ� ���ͷ�Ʈ ��ƾ
// ����� �������� �ϳ��� ���ٸ� �������� �ε����� �ʴ´�.
// ����ġ�� ������ LCD�� ������ ���� ������ ��µȴ�. ��ư�� ���� ������ �������� 1���� ����� ���� ��ŭ ���� ���õǰ�
// �������� ������ �Ŀ� 2�ʰ� ����ϸ� ������ ���������� ���ļ��� �����ǰ� �ȴ�. 
interrupt[EXT_INT3] void LoadPresetSwitchOn(void)
{
    if(maxPresetNum == 0){                                          // �������� ������ ���� ���ٸ�
        LCD_ClearLine2();
        LCD_WriteCommand(LINE2);                                    // "no preset saved"��� ������ 2�ʰ� ����
        LCD_PrintString("no preset saved");
        delay_ms(2000);
        LCD_ClearLine2();
        LCD_ShowFreq(frequency);                                    // 2�� �Ŀ��� �ٽ� ���ļ��� ��ȣ ������ ����Ѵ�.
        LCD_UpdateSignalStatus(status.signalLVL);
    }
    
    else{                                                           // �������� ������ ���� �ִٸ�
        isPresetLoaded = 0;                                         
        presetNum++;                                                // �����¹�ȣ�� 1�� �����Ѵ�.
        if(presetNum > maxPresetNum){                               
            presetNum = 1;                                          // �����¹�ȣ�� max���� �ʰ����� �� �ٽ� 1�� �����Ѵ�.
        }
        
        LCD_ClearLine2();
        LCD_WriteCommand(LINE2);
        sprintf(str_buf, "selected:PRESET%d", presetNum);           // ���õ� ������ ���� LCD�� ����Ѵ�.
        LCD_PrintString(str_buf);
        
        EIMSK = 0b00001000;                                         // �ܺ����ͷ�Ʈ 3�� ���� Ȱ��ȭ ��Ű��
        SREG = 0x80;                                                // ���� ���ͷ�Ʈ�� ��� Ȱ��ȭ ��Ų �Ŀ� 2�ʰ���ٸ���.
        delay_ms(2000);                                             // 2�� ���ȿ� �� ��ư�� �ٽ� �����ٸ� �ٽ� ���ͷ�Ʈ�� �߻��Ͽ�
                                                                    // ������ 2, 3, 4 ���� ������ �� �ְ� �ȴ�.
        
        if(isPresetLoaded == 0){
            isLoadingPreset = 1;                                    // �������� ���� ���ļ� ���� ���� true
            isPresetLoaded = 1;                                     // �����·ε带 �ϰ� �Ǿ����� ���� true(���ͷ�Ʈ ��ø ����)
            SREG = 0x00;                                            // ���� ���ͷ�Ʈ�� ��Ȱ��ȭ �ϰ�
            EIMSK = 0b11111100;                                     // EXT_INT 2, 3, 4, 5, 6, 7 Enable
            TEA5767_SetFreq(preset_frequency[presetNum-1]);         // 
            presetNum = 0;                                          // ������ �ѹ��� �ٽ� 0���� �ʱ�ȭ
            TEA5767_WriteData();                                    // ���õ� �����¿� �ش��ϴ� ���ļ��� Ʃ��
            LCD_ClearLine2();                                       //
            LCD_ShowFreq(frequency);                                // ������ ä���� ���ļ����� ����ϰ�
            delay_ms(1000);                                         // ����� �ð��� ��ٸ��� ������ ä�ο� ���� 
            TEA5767_GetStatus();                                    // ��� ���¸� �о�´�
            LCD_UpdateSignalStatus(status.signalLVL);               // �׸��� ��� ���¸� ����Ѵ�.
        }
    }
    EIFR = 0xFF;                                                    // ���߿� �߻��� ���ͷ�Ʈ�� ��� �ʱ�ȭ ��Ų��.
}


// RotaryEncoder�� ȸ���ϸ� �߻��ϴ� ���ͷ�Ʈ ��ƾ
// ��Ī���: TEA5767���� ������ ��Ī ���⿡ ���� ä���� Ž���ϰ� �Ǵµ� ��½�ȣ ������ ���� �������� ���� �� �� ä�η� Ʃ���ϴ� ���.
//        ����� ��½�ȣ ������ High(���������� 10)�� �Ѵ� ���ļ��� Ž���ϵ��� TEA5767_Init()�Լ����� �ʱ�ȭ ���־���
// RotaryEncoder�� ��긦 ������ ȸ�� ���⿡ ���� ���ļ��� ������Ű�ų� ���ҽ�Ű�鼭 ������ Ʃ���Ѵ�.
// ��Ī��尡 Ȱ��ȭ �Ǿ� �ִ� ���¶�� ȸ�� ���⿡ ���� ��Ī�� �����Ѵ�.
interrupt[EXT_INT4] void RotaryEncode(void)
{
    frequency_old = frequency;                                      // Ʃ�� ������ ���ļ��� �����´�.
                
    if (PINE.3){                                                    // PINE.3�� �Էµ� ���� 1�̶�� ���������� ȸ���ϴ°��̴�.
        write_bytes[2] |= 0x80;                                     // ��ġ����� ������ �������� �����Ѵ�(��Ī����϶��� ���ǹ�).
        TEA5767_SetFreq( frequency + 1 );                           // ���� ���ļ����� 0.1Mhz���� ���ļ��� �����Ѵ�.
        isSearchingUp = 1;                                          // ���ļ��� �����ϴ� �������� ��Ī���� yes(��Ī����϶��� ���ǹ�)
    }
    else {                                                          // PINE.3�� �Էµ� ���� 0�̶�� �������� ȭ���ϴ°��̴�.
        write_bytes[2] &= ~0x80;                                    // ��ġ����� ������ �Ʒ������� �����Ѵ�(��Ī����϶��� ���ǹ�).
        TEA5767_SetFreq( frequency - 1 );                           // ���� ���ļ����� 0.1Mhz���� ���ļ��� �����Ѵ�.
        isSearchingUp = 0;                                          // ���ļ��� �����ϴ� �������� ��Ī���� no(��Ī����϶��� ���ǹ�)
    }           
    TEA5767_WriteData();                                            // Write data to TEA5767
                
    if(searchModeOn != 0){                                          // ��ġ����� 
        delay_ms(500);                                              // ��Ī �Ϸ�� ������ ����� �ð��� ��ٸ���
        TEA5767_GetStatus();                                        // ã�Ƴ� ä���� ���ļ��� �����´�.
        frequency = status.currentFreq;                             // �� ���ļ��� ���� ���ļ� ������ �����Ѵ�.
        // ���ļ� ��Ī ��忡 ���� LCD �ִϸ��̼��� �����ϱ� ���� �ڵ�
        if(isSearchingUp == 1){                                     // ã�� ������ ���ļ��� �����ϴ� �����̸�
            for( i = frequency_old; i != frequency + 1; i++){       // ��Ī ��� ������ ���ļ����� ��Ī �Ϸ�� ���ļ� ���� i�� ������Ű�鼭
                if(i > 1080){                                       // LCD�� ��µ� ������ �ٴ��� �����̰� �Ѵ�.
                    i = 875;                                        // 1080�� �ʰ��ϰ� �Ǹ� 875���� i�� �����ϵ��� �Ѵ�.
                }
                delay_ms(10);
                LCD_UpdateScale(i);                                 // i���� ���� �ٴ��� �����̰� �ϴ� �Լ�
            }
        }
        else{                                                       // ã�� ������ ���ļ��� �����ϴ� �����̶��
            for( i = frequency_old; i != frequency - 1; i--){       // ��Ī ��� ������ ���ļ����� ��Ī �Ϸ�� ���ļ� ���� i�� ������Ű�鼭
                if(i < 875){                                        // LCD�� ��µ� ������ �ٴ��� �����̰� �Ѵ�.
                    i = 1080;                                       // 1080�� �ʰ��ϰ� �Ǹ� 875���� i�� �����ϵ��� �Ѵ�.
                }                                                   
                delay_ms(10);                                       
                LCD_UpdateScale(i);                                 // i���� ���� �ٴ��� �����̰� �ϴ� �Լ�
            }
        }
        delay_ms(500);                                              // 500ms�̻��� ����� �ð� �Ŀ�
        TEA5767_GetStatus();                                        // ���� ä���� ��ȣ ���⸦ �޾ƿͼ�
        LCD_UpdateSignalStatus(status.signalLVL);                   // LCD�� ���⸦ ��Ÿ����.
    }                                                                                                           
    
    else{                                                           // ��ġ��尡 �ƴ϶��
        delay_ms(100);                                              // ���� ä�� ������ ���� 100ms�� ��ٸ���
        TEA5767_GetStatus();                                        // ���� ä���� ��ȣ ���⸦ �޾ƿͼ�
        LCD_UpdateSignalStatus(status.signalLVL);                   // LCD�� ���⸦ ��Ÿ����.
    }
    EIFR = 0xFF;                                                    // ��Ȯ�� ȸ�� ������ �о���� ���ؼ� ���ͷ�Ʈ ��ø�� ������� �ʴ´�.
}           

// RotaryEncoder�� ����� ��Ʈ ����ġ�� ������ �߻��ϴ� ���ͷ�Ʈ ���� ��ƾ
// ��ġ��� ON/OFF����� ����Ѵ�.
interrupt[EXT_INT5] void RotarySwitchOn(void)           
{           
    if(searchModeOn == 0){                                          // ���� ��ġ��尡 �ƴϾ��ٸ�                           
        write_bytes[0] |= 0x40;                                     // Search Flag�� set���� ��ġ��� ON
        searchModeOn = 1;                                           // ��ġ��� ���� true
    }           
    else{                                                           // ���� ��ġ��� ���ٸ�
        write_bytes[0] &= ~0x40;                                    // Search Flag�� clear���� ��ġ��� OFF
        searchModeOn = 0;                                           // ��ġ��� ���� false
    }
}

// ��Ʈ ����ġ�� �������� �߻��ϴ� ���ͷ�Ʈ ���� ��ƾ
// ��Ʈ ON/OFF����� ����Ѵ�.
interrupt[EXT_INT6] void MuteSwitchOn(void)
{
    if(muteOn == 0){                                                // ���� ��Ʈ�� ON�� �ƴϾ��ٸ�
        write_bytes[0] |= 0x80;                                     // Mute on Flag�� set���� ��Ʈ ON
        muteOn = 1;                                                 // ��Ʈ ���� true
        PORTB &= ~0x10;                                             // PB0�� ����� ���� LED ON
    }
    else{                                                           // ���� ��Ʈ�� OFF���ٸ�
        write_bytes[0] &= ~0x80;                                    // Mute on Flag�� clear���� ��Ʈ OFF
        muteOn = 0;                                                 // ��Ʈ ���� false
        PORTB |= 0x10;                                              // PB0�� ����� ���� LED OFF
    }
    TEA5767_WriteData();                                            // ��Ʈ ON/OFF ����
}


// ���� ��� ��� ����ġ�� �������� �߻��ϴ� ���ͷ�Ʈ ���� ��ƾ
// ���� ��� ��� ON/OFF����� ����Ѵ�.
interrupt[EXT_INT7] void ForcedMonoSwitchOn(void)
{
    if(forcedMonoModOn == 0){                                       // ���� ���� ��� ��� ON�� �ƴϾ��ٸ�
        write_bytes[2] |= 0x08;                                     // Forced Mono Flag�� set���� ���� ��� ON
        forcedMonoModOn = 1;                                        // ���� ��� ��� ��� ���� true
        PORTB &= ~0x20;                                             // PB0�� ����� ���� LED ON
    }                                                               
    else{                                                           // ���� ��Ʈ�� OFF���ٸ�
        write_bytes[2] &= ~0x08;                                    // Forced Mono Flag�� clear���� ���� ���OFF
        forcedMonoModOn = 0;                                        // ���� ��� ��� ��� ���� false
        PORTB |= 0x20;                                              // PB0�� ����� ���� LED OFF
    }                                                               
    TEA5767_WriteData();                                            // ���� ��� ��� ��� ON/OFF ����
}