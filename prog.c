/*
 * File:   prog.c
 * Author: vadim
 *
 * Created on 24 августа 2016 г., 13:33
 */

#include <stdio.h>
#include <stdlib.h>
#include <pic.h>
#include <xc.h>

// CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSC oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = ON       // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is MCLR)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

volatile unsigned char digits [3];
volatile unsigned char digits_0 [3];
volatile unsigned char KeyCode;
const unsigned char TMR0_VALUE = 235;
unsigned char digitnum;
unsigned char digitemp;
int powerOnInterval;
unsigned char count2 = 0;
const unsigned char PortAData[3] = {
	0b10000000,
	0b01000000,
	0b00000001,
};
bit endInterrupt;
bit powerOff = 0;
bit Broadcasting;

unsigned char DS_Adress [] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char temperature = 0; //температура
unsigned char temp_drob = 0; //дробная часть температуры
unsigned char sign; //знак температуры

#define key1 RA2
#define key2 RA1

#define STATE TRISA3
#define PIN RA3

#define  CELL_CAPACITY (sizeof(DS_Adress))
#define END_OF_CELLS (CELL_CAPACITY * 24)
#define LAST_CELL (END_OF_CELLS - CELL_CAPACITY)


#define _XTAL_FREQ 4000000

void Reset_powerOnInterval(){
	powerOnInterval = 700;
}
unsigned char getDigit(char a) {
	return (digits_0[a - 1] & 0b00111111);
}

void setDigit(char a, unsigned char data) {
	digits_0[a - 1] = (digits_0[a - 1] & 0b11000000) | (data & 0b00111111);
}

void setPoint(char dignum, char value) {
	value == 1 ? digits_0[dignum - 1] |= 0b01000000 : digits_0[dignum - 1] &= 0b10111111;
}

void refreshInd() {
	for (char a = 0; a < 3; a++) {
		digits[a] = digits_0[a];
	}
}

void clrInd() {
	for (char a = 0; a < 3; a++) {
		digits_0[a] = 34;
	}
}

unsigned char convDig(unsigned char dig) {
	switch (dig) {
		case 0: return 0b11011011; //0
		case 1: return 0b00011000; //1
		case 2: return 0b10110011; //2
		case 3: return 0b10111010; //3
		case 4: return 0b01111000; //4
		case 5: return 0b11101010; //5
		case 6: return 0b11101011; //6
		case 7: return 0b10011000; //7
		case 8: return 0b11111011; //8
		case 9: return 0b11111010; //9
		case 10: return 0b11111001; //A
		case 11: return 0b01101011; //B
		case 12: return 0b11000011; //C
		case 13: return 0b00111011; //D
		case 14: return 0b11100011; //E
		case 15: return 0b11100001; //F
		case 16: return 0b11111000; //G
		case 17: return 0b01101001; //H
		case 18: return 0b01000001; //I
		case 19: return 0b00011010; //J
		case 20: return 0b01000011; //L
		case 21: return 0b00101001; //N
		case 22: return 0b00101011; //O
		case 23: return 0b11110001; //P
		case 24: return 0b00100001; //R
		case 25: return 0b01101010; //S
		case 26: return 0b01100011; //T
		case 27: return 0b01011011; //U
		case 28: return 0b00001011; //V
		case 29: return 0b01110001; //Y
		case 30: return 0b10110001; //Z
		case 31: return 0b11110000; //°
		case 32: return 0b00100000; //–
		case 33: return 0b00000010; //_
		case 34: return 0b00000000; //empty
		default: return 0b00000000; //empty
	}
}//


void EEWR(unsigned char adress, unsigned char data) {
	volatile unsigned char INTCON_BUP = INTCON;
	INTCONbits.GIE = 0;
	EEADR = adress;
	EEDATA = data;
	EECON1bits.WREN = 1;
	EECON2 = 0x55;
	EECON2 = 0xAA;
	EECON1bits.WR = 1;
	EECON1bits.WREN = 0;
	while (EECON1bits.WR == 1) {
	}
	INTCON = INTCON_BUP;
}

unsigned char EERD(unsigned char adress) {
	volatile unsigned char INTCON_BUP = INTCON;
	volatile unsigned char EEDATA_BUP;
	INTCONbits.GIE = 0;
	EEADR = adress;
	EECON1bits.RD = 1;
	EEDATA_BUP = EEDATA;
	INTCON = INTCON_BUP;
	return EEDATA_BUP;
}

void FillArrayFromEEPROM(unsigned char *container, unsigned char adress_start, unsigned char quantity) {
	for (unsigned char i = 0; i < quantity; i++) {
		container[i] = EERD(adress_start + i);
	}
}

void WriteArrayToEEPROM(unsigned char * container, unsigned char adress_start, unsigned char quantity) {
	for (unsigned char i = 0; i < quantity; i++) {
		EEWR(adress_start + i, container[i]);
	}
}

void LoadAdress(unsigned char cell){
	FillArrayFromEEPROM(DS_Adress, cell * sizeof(DS_Adress), sizeof(DS_Adress));
}

/* Функции протокола 1-wire */
static bit INIT() {
	static bit b;
	b = 0;
	STATE = 1;
	__delay_us(20);
	STATE = 0;
	__delay_us(500);
	STATE = 1;
	__delay_us(65);
	b = PIN;
	__delay_us(450);

	return !b;
}

void TX(unsigned char cmd) {
	unsigned char temp = 0;
	unsigned char i = 0;
	temp = cmd;
	for (i = 0; i < 8; i++) {
		if (temp & 0x01) {
			STATE = 0;
			__delay_us(5);
			STATE = 1;
			__delay_us(70);
		} else {
			STATE = 0;
			__delay_us(70);
			STATE = 1;
			__delay_us(5);
		}
		temp >>= 1;
	}
}

unsigned char RX() {
	unsigned char d = 0;
	for (unsigned char i = 0; i < 8; i++) {
		STATE = 0;
		__delay_us(6);
		STATE = 1;
		__delay_us(4);
		d >>= 1;
		if (PIN == 1) d |= 0x80;
		__delay_us(60);
	}
	return d;
}//

void Send_DS_Adress(){
	if(Broadcasting){
		TX(0xCC);
	}else{
		TX(0x55);
		for(unsigned char i = 0; i < sizeof(DS_Adress); i++){
			TX(DS_Adress[i]);
		}
	}
}
/* Получаем значение температуры */
void get_temp() {
	static bit init;
	unsigned char temp1 = 0;
	unsigned char temp2 = 0;
	init = INIT();

	endInterrupt = 0;
	while (!endInterrupt);

	if (init) {
		Send_DS_Adress();
		TX(0x44);
		__delay_ms(250);
		__delay_ms(250);
		__delay_ms(250);
		__delay_ms(250);
	}

	endInterrupt = 0;
	while (!endInterrupt);

	init = INIT();

	endInterrupt = 0;
	while (!endInterrupt);

	if (init) {
		Send_DS_Adress();
		TX(0xBE);

		endInterrupt = 0;
		while (!endInterrupt);

		temp1 = RX();
		temp2 = RX();
	}
	temp_drob = temp1 & 0b00001111; //Записываем дробную часть в отдельную переменную
	temp_drob = ((temp_drob * 6) + 2) / 10; //Переводим в нужное дробное число
	temp1 >>= 4;
	sign = temp2 & 0x80;
	temp2 <<= 4;
	temp2 &= 0b01110000;
	temp2 |= temp1;

	if (sign) {
		temperature = 127 - temp2;
		temp_drob = 10 - temp_drob;
		if (temp_drob == 10) {
			temp_drob = 0;
			temperature++;
		}
	} else {
		temperature = temp2;
	}
}//

unsigned char FindCell(unsigned char adressStart, unsigned char previous) {
	if (adressStart == END_OF_CELLS) {
		adressStart = LAST_CELL;
	}
	unsigned int adress = adressStart;
	unsigned int adressNew = END_OF_CELLS;
	do {

		if (!previous) {
			adress == LAST_CELL ? adress = 0 : adress += CELL_CAPACITY;
		} else {
			adress == 0 ? adress = LAST_CELL : adress -= CELL_CAPACITY;
		}

		unsigned char CellsData [CELL_CAPACITY];
		FillArrayFromEEPROM(&CellsData, adress * CELL_CAPACITY, CELL_CAPACITY);
		//ReadDataOfCell(&CellsData, &Data, adress);
		unsigned char CellIsEmpty = 1;
		for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
			if (CellsData[i] != 0xFF) {
				CellIsEmpty = 0;
			}
		}

		if (CellIsEmpty == 0) {
			adressNew = adress;
			break;
		}
	} while (adress != adressStart);
	return adressNew;
}

//////////

void interrupt F() {
	if (T0IF) {
		T0IF = 0;
		TMR0 += TMR0_VALUE;
		
		if (count2 > 2)count2 = 0;
		digitnum = PortAData[count2];
		unsigned char dig = digits[count2];
		digitemp = convDig(0b00111111 & dig);
		/*	if (0b10000000==(0b10000000&dig)){
				if(!blink) digitemp = 0;
			}*/
		(0b01000000 == (0b01000000 & dig)) ? digitemp |= 0b00000100 : digitemp &= 0b11111011;
		count2++;

		PORTB = digitemp;
		PORTA = (PORTA & 0b00110110) | digitnum;
		endInterrupt = 1;
		powerOnInterval--;
		if (powerOnInterval == 0) {
			powerOff = 1;
			TRISA4 = 1;
		}

		static unsigned int KeyTimeCounter = 0;
		static unsigned char LastKeysState = 0;
		static unsigned char long_press = 0;
		static unsigned char ButtonPressTimeIn1 = 0;
		static unsigned char ButtonPressTimeIn2 = 0;
		static unsigned char ButtonPressTimeOut = 0;
		unsigned char CurrentKeysState = 0;
		
		if (ButtonPressTimeOut > 0) {
			ButtonPressTimeOut--;
		} else {
			
			KeyCode = 0;
			
			if(key1){
				CurrentKeysState = CurrentKeysState | 0b00000001;
				ButtonPressTimeIn1 = 25;
			}else{
				if(ButtonPressTimeIn1 == 0){
					CurrentKeysState = CurrentKeysState & 0b11111110;
				}else{
					ButtonPressTimeIn1--;
				}
			}

			if(key2){
				CurrentKeysState = CurrentKeysState | 0b00000010;
				ButtonPressTimeIn2 = 25;
			}else{
				if(ButtonPressTimeIn2 == 0){
					CurrentKeysState = CurrentKeysState & 0b11111101;
				}else{
					ButtonPressTimeIn2--;
				}
			}
			
			if((ButtonPressTimeIn1 == 0 || ButtonPressTimeIn1 == 25) && (ButtonPressTimeIn2 == 0 || ButtonPressTimeIn2 == 25)){
				if(CurrentKeysState > 0){
					if(LastKeysState != CurrentKeysState){
						KeyTimeCounter = 0;
						long_press = 0;
						LastKeysState = CurrentKeysState;
					}else if(KeyTimeCounter < 150){
						KeyTimeCounter++;
					}else if(KeyTimeCounter == 150 && !long_press){
						long_press = 1;
					}

					if(long_press == 1){
						KeyCode = 30 + LastKeysState + 3;
						long_press = 2;
						ButtonPressTimeOut = 40;
					}

				}else if(LastKeysState > 0 && long_press == 0){
					KeyCode = 30 + LastKeysState;
					LastKeysState = 0;
					KeyTimeCounter = 0;
					long_press = 0;
					ButtonPressTimeOut = 40;

				}else if(long_press == 2){
					LastKeysState = 0;
					KeyTimeCounter = 0;
					long_press = 0;
					ButtonPressTimeOut = 40;
				}
			}
		}
	}
}

void indData() {

	if (!INIT()) {
		setDigit(3, 14);
		setDigit(2, 24);
		setDigit(1, 24);
		return;
	}

	unsigned char cd = 3;
	unsigned char dN = 3;

	if (sign != 0) {
		setDigit(cd, 32);
		cd--;
		dN--;
	}
	unsigned char isPoint = 0;
	do {
		unsigned char v = (temperature > 9 ? 1 : 0) + (temperature > 99 ? 1 : 0);
		setDigit(cd - v, temperature % 10);
		if (!isPoint) {
			setPoint(cd - v, 1);
			isPoint = 1;
		}
		temperature /= 10;
		dN--;
	} while (temperature > 0);

	if (dN > 0) {
		cd = dN;
		if (temp_drob > 99) temp_drob /= 10;
		if (temp_drob > 9 && dN == 1) temp_drob /= 10;
		do {
			unsigned char v = (temp_drob > 9 ? 1 : 0);
			setDigit(cd - v, temp_drob % 10);
			temp_drob /= 10;
			dN--;
		} while (temp_drob > 0);
	}

	while (dN > 0) {
		setDigit(dN, 0);
		dN--;
	}
}

void main() {
	//powerOn = 1;
	
	INTCON = 0;
	OPTION_REG = 0b00000111;
	TRISA = 0b00000110;
	TRISB = 0b00000000;
	PORTA = 0b00000000;
	PORTB = 0b00000000;
	TMR0 = TMR0_VALUE;
	T2CON = 0b00000100;
	CMCON = 0b00000111;
	INTCON = 0b10100000;
	
	Reset_powerOnInterval();
	
	clrInd();
	setDigit(1, 32);
	setDigit(2, 32);
	setDigit(3, 32);
	
	refreshInd();
	
	/*INIT();
	TX(0xCC);
	TX(0x44);*/

	unsigned char adress = FindCell(END_OF_CELLS, 0);
	Broadcasting = adress == END_OF_CELLS;
	
	if(Broadcasting){
		setPoint(2, 1);
	}
	refreshInd();
	
	Broadcasting = 1;
	
	INIT();
	TX(0x33);
	for(unsigned char i = 0; i < CELL_CAPACITY; i++){
		DS_Adress[i] = RX();
	}
	WriteArrayToEEPROM(DS_Adress, 0, CELL_CAPACITY);
	
	
	while (1) {
		
		
		
		/*
		if(KeyCode == 30 || KeyCode == 31 || start == 1){
			unsigned char direct = 255;
			while(1){
				if(KeyCode == 30){
					KeyCode = 0;
					Reset_powerOnInterval();
					direct = 0;
				}else if(KeyCode == 31){
					KeyCode = 0;
					Reset_powerOnInterval();
					direct = 1;
				}else if(start == 1){
					start = 0;
					//Reset_powerOnInterval();
					direct = 1;
				}
				
				unsigned char run = 1;
				if(cell > 0 && direct == 0){
					cell--;
				}else if(cell < 7 && direct == 1){
					cell++;
				}else{
					run = 0;
				}
				
				if(run){
					LoadAdress(cell);
					Broadcasting = 1;
					for(unsigned char i = 0; i < sizeof(DS_Adress); i++){
						if(DS_Adress[i] != 0xFF){
							run = 0;
							Broadcasting = 0;
							break;
						}
					}
				}else{
					break;
				}
				
				if(!run){
					break;
				}
			}
		}
		*/
		/*
		if(KeyCode == 36){
			KeyCode = 0;
			
		}
		
		clrInd();
		endInterrupt = 0;
		while (!endInterrupt);
		get_temp();
		indData();
		for (unsigned char i = 0; i++; i <= 10) {
			__delay_ms(200);
		}
		refreshInd();
	*/
	}
}