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

//__EEPROM_DATA (0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
//__EEPROM_DATA (0x28, 0xFF, 0xBE, 0xAC, 0x64, 0x15, 0x01, 0x68);
//__EEPROM_DATA (0x28, 0xFF, 0x13, 0xE7, 0x63, 0x15, 0x02, 0x5B);

volatile unsigned char digits [3];
volatile unsigned char digits_0 [3];
volatile unsigned char KeyCode;
const unsigned char TMR0_VALUE = 235;
unsigned char digitnum;
unsigned char digitemp;
int powerOnInterval;
unsigned char DigitNumber = 0;
const unsigned char PortAData[3] = {
	0b10000000,
	0b01000000,
	0b00000001,
};
bit endInterrupt;
bit powerOff = 0;
bit Broadcasting;
unsigned char PowerBlocked;

struct {
	unsigned Init : 1;
	unsigned Send_Address : 1;
	unsigned CountAddressBytes : 4;
	unsigned SendConvertTemp : 1;
	unsigned int PauseValue;
	unsigned SendGetTemp : 1;
	unsigned ReadData : 1;
	unsigned CountDataBytes : 4;
	unsigned Error : 1;
	unsigned ActiveProcess : 1;
} getTemp_flags;

unsigned char DS_Address [] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char DS_ReadData [] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char temperature = 0; //температура
unsigned char temp_drob = 0; //дробная часть температуры
unsigned char sign; //знак температуры

#define key1 RA2
#define key2 RA1

#define STATE TRISA3
#define PIN RA3

#define CELLS_COUNT 16
#define CELL_CAPACITY (sizeof(DS_Address))
#define END_OF_CELLS (CELL_CAPACITY * CELLS_COUNT)
#define LAST_CELL (END_OF_CELLS - CELL_CAPACITY)

#define _XTAL_FREQ 4000000

void waitInterrupt() {
	endInterrupt = 0;
	while (!endInterrupt);
}

void Reset_powerOnInterval() {
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

void ShowError() {
	clrInd();
	setDigit(3, 14);
	setDigit(2, 24);
	setDigit(1, 24);
	refreshInd();
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

void EEWR(unsigned char address, unsigned char data) {
	volatile unsigned char INTCON_BUP = INTCON;
	INTCONbits.GIE = 0;
	EEADR = address;
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

unsigned char EERD(unsigned char address) {
	volatile unsigned char INTCON_BUP = INTCON;
	volatile unsigned char EEDATA_BUP;
	INTCONbits.GIE = 0;
	EEADR = address;
	EECON1bits.RD = 1;
	EEDATA_BUP = EEDATA;
	INTCON = INTCON_BUP;
	return EEDATA_BUP;
}

void FillArrayFromEEPROM(unsigned char *container, unsigned char address_start, unsigned char quantity) {
	for (unsigned char i = 0; i < quantity; i++) {
		//waitInterrupt();
		container[i] = EERD(address_start + i);
	}
}

void WriteArrayToEEPROM(unsigned char * container, unsigned char address_start, unsigned char quantity) {
	for (unsigned char i = 0; i < quantity; i++) {
		waitInterrupt();
		EEWR(address_start + i, container[i]);
	}
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

unsigned char calc_crc(unsigned char *mas, unsigned char len) {
	unsigned char crc = 0;
	while (len--) {
		unsigned char dat = *mas++;
		for (unsigned char i = 0; i < 8; i++) { // счетчик битов в байте
			unsigned char fb = (crc ^ dat) & 1;
			crc >>= 1;
			dat >>= 1;
			if (fb) crc ^= 0x8c; // полином
		}
	}
	return crc;
}

void Send_DS_Address() {
	waitInterrupt();
	if (Broadcasting) {
		TX(0xCC);
	} else {
		TX(0x55);
		for (unsigned char i = 0; i < sizeof (DS_Address); i++) {
			waitInterrupt();
			TX(DS_Address[i]);
		}
	}
}

/* Получаем значение температуры */
void get_temp() {

	temperature = 72;
	temp_drob = 0;

	return;

	static bit init;
	unsigned char temp1 = 0;
	unsigned char temp2 = 0;
	init = INIT();

	endInterrupt = 0;
	while (!endInterrupt);

	if (init) {
		TX(0xCC);
		//Send_DS_Address();
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
		Send_DS_Address();
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
}

void get_temp_Async() {

	if (!getTemp_flags.ActiveProcess) {
		return;
	}

	// Step 1, 5
	if (getTemp_flags.Init) {
		if (INIT()) {
			getTemp_flags.Init = 0;

			getTemp_flags.Send_Address = 1;
			getTemp_flags.CountAddressBytes = 0;
		} else {
			getTemp_flags.ActiveProcess = 0;
			getTemp_flags.Error = 1;
		}
	} else
		// Step 2, 6
		if (getTemp_flags.Send_Address) {
		if (Broadcasting || getTemp_flags.SendConvertTemp) {
			TX(0xCC);
			getTemp_flags.CountAddressBytes = 1;
			getTemp_flags.Send_Address = 0;
		} else if (getTemp_flags.CountAddressBytes < sizeof (DS_Address)) {
			if (getTemp_flags.CountAddressBytes == 0) {
				TX(0x55);
			}
			TX(DS_Address[getTemp_flags.CountAddressBytes]);
			getTemp_flags.CountAddressBytes++;

			if (getTemp_flags.CountAddressBytes == sizeof (DS_Address)) {
				getTemp_flags.Send_Address = 0;
			}
		}
	} else
		// Step 3
		if (getTemp_flags.SendConvertTemp) {
		TX(0x44);
		getTemp_flags.SendConvertTemp = 0;

	} else
		// Step 4
		if (getTemp_flags.PauseValue > 0) {
		getTemp_flags.PauseValue--;
		if (getTemp_flags.PauseValue == 0) {
			getTemp_flags.Init = 1;
		}
	} else
		// Step 7
		if (getTemp_flags.SendGetTemp) {
		TX(0xBE);
		getTemp_flags.SendGetTemp = 0;
	} else
		// Step 8
		if (getTemp_flags.ReadData) {
		if (getTemp_flags.CountDataBytes < sizeof (DS_ReadData)) {
			for (unsigned char i = 0; i < 3 && getTemp_flags.CountDataBytes < sizeof (DS_ReadData); i++) {
				DS_ReadData[getTemp_flags.CountDataBytes] = RX();
				getTemp_flags.CountDataBytes++;
			}
			if (getTemp_flags.CountDataBytes == sizeof (DS_ReadData)) {
				if (DS_ReadData[sizeof (DS_ReadData) - 1] != calc_crc(DS_ReadData, sizeof (DS_ReadData) - 1)) {
					getTemp_flags.Error = 1;
				} else {

					unsigned char temp1 = DS_ReadData[0];
					unsigned char temp2 = DS_ReadData[1];

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
					
				}
				getTemp_flags.ReadData = 0;
				getTemp_flags.ActiveProcess = 0;
			}
		}
	}
}

unsigned char FindCell(unsigned char addressStart, unsigned char previous) {
	if (addressStart == END_OF_CELLS) {
		previous ? addressStart = 0 : addressStart = LAST_CELL;
	}
	unsigned char address = addressStart;
	unsigned char addressNew = END_OF_CELLS;
	do {

		if (!previous) {
			address == LAST_CELL ? address = 0 : address += CELL_CAPACITY;
		} else {
			address == 0 ? address = LAST_CELL : address -= CELL_CAPACITY;
		}

		unsigned char CellsData [CELL_CAPACITY];
		waitInterrupt();
		FillArrayFromEEPROM(CellsData, address, CELL_CAPACITY);
		unsigned char CellIsEmpty = 1;
		for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
			if (CellsData[i] != 0xFF) {
				CellIsEmpty = 0;
			}
		}

		if (CellIsEmpty == 0) {
			addressNew = address;
			break;
		}
	} while (address != addressStart);
	return addressNew;
}

//////////

void interrupt F() {
	if (T0IF) {

		T0IF = 0;
		TMR0 += TMR0_VALUE;

		if (DigitNumber > 2)DigitNumber = 0;
		digitnum = PortAData[DigitNumber];
		unsigned char dig = digits[DigitNumber];
		digitemp = convDig(0b00111111 & dig);
		/*	if (0b10000000==(0b10000000&dig)){
				if(!blink) digitemp = 0;
			}*/
		(0b01000000 == (0b01000000 & dig)) ? digitemp |= 0b00000100 : digitemp &= 0b11111011;
		DigitNumber++;

		PORTB = digitemp;
		PORTA = (PORTA & 0b00110110) | digitnum;
		endInterrupt = 1;
		powerOnInterval--;
		if (powerOnInterval == 0 && PowerBlocked == 0) {
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

			if (key1) {
				CurrentKeysState = CurrentKeysState | 0b00000001;
				ButtonPressTimeIn1 = 25;
			} else {
				if (ButtonPressTimeIn1 == 0) {
					CurrentKeysState = CurrentKeysState & 0b11111110;
				} else {
					ButtonPressTimeIn1--;
				}
			}

			if (key2) {
				CurrentKeysState = CurrentKeysState | 0b00000010;
				ButtonPressTimeIn2 = 25;
			} else {
				if (ButtonPressTimeIn2 == 0) {
					CurrentKeysState = CurrentKeysState & 0b11111101;
				} else {
					ButtonPressTimeIn2--;
				}
			}

			if ((ButtonPressTimeIn1 == 0 || ButtonPressTimeIn1 == 25) && (ButtonPressTimeIn2 == 0 || ButtonPressTimeIn2 == 25)) {
				if (CurrentKeysState > 0) {
					if (LastKeysState != CurrentKeysState) {
						KeyTimeCounter = 0;
						long_press = 0;
						LastKeysState = CurrentKeysState;
					} else if (KeyTimeCounter < 150) {
						KeyTimeCounter++;
					} else if (KeyTimeCounter == 150 && !long_press) {
						long_press = 1;
					}

					if (long_press == 1) {
						KeyCode = 30 + LastKeysState + 3;
						long_press = 2;
						ButtonPressTimeOut = 40;
					}

				} else if (LastKeysState > 0 && long_press == 0) {
					KeyCode = 30 + LastKeysState;
					LastKeysState = 0;
					KeyTimeCounter = 0;
					long_press = 0;
					ButtonPressTimeOut = 40;

				} else if (long_press == 2) {
					LastKeysState = 0;
					KeyTimeCounter = 0;
					long_press = 0;
					ButtonPressTimeOut = 40;
				}
			}
		}

		get_temp_Async();
	}
}

void indData() {

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

void ReadCell(unsigned char cell, unsigned char * CellsData, unsigned char * CellIsEmpty) {
	unsigned char _CellIsEmpty = 1;
	FillArrayFromEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
	for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
		if (CellsData[i] != 0xFF) {
			_CellIsEmpty = 0;
			break;
		}
	}
	*CellIsEmpty = _CellIsEmpty;
}

void CellToInd(unsigned char cell) {
	clrInd();

	if (cell >= 9) {
		setDigit(3, (1 + cell) / 10);
		setDigit(2, (1 + cell) % 10);
	} else {
		setDigit(3, 1 + cell);
		setDigit(2, 34);
	}
	setDigit(1, 34);

	refreshInd();
}

void EditAddressMemory() {
	getTemp_flags.ActiveProcess = 0;

	PowerBlocked++;

	clrInd();

	unsigned char cell = 0;
	unsigned char CellIsEmpty = 1;
	unsigned char CellsData [CELL_CAPACITY];

	ReadCell(cell, CellsData, &CellIsEmpty);
	CellToInd(cell);

	while (1) {
		if (KeyCode == 31) {
			KeyCode = 0;
			if (cell > 0) {
				cell--;
			} else {
				cell = CELLS_COUNT - 1;
			}
			ReadCell(cell, CellsData, &CellIsEmpty);
			CellToInd(cell);
		} else if (KeyCode == 32) {
			KeyCode = 0;
			if (cell < CELLS_COUNT - 1) {
				cell++;
			} else {
				cell = 0;
			}
			ReadCell(cell, CellsData, &CellIsEmpty);
			CellToInd(cell);
		} else if (KeyCode == 34) {
			KeyCode = 0;
			clrInd();
			refreshInd();
			break;
		} else if (KeyCode == 35 && CellIsEmpty == 1) {
			KeyCode = 0;
			waitInterrupt();
			if (INIT()) {
				waitInterrupt();
				TX(0x33);
				waitInterrupt();
				unsigned char CellsData [CELL_CAPACITY];
				for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
					waitInterrupt();
					CellsData[i] = RX();
				}
				if (CellsData[CELL_CAPACITY - 1] == calc_crc(CellsData, CELL_CAPACITY - 1)) {
					waitInterrupt();
					WriteArrayToEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
					waitInterrupt();
					ReadCell(cell, CellsData, &CellIsEmpty);
				} else {
					ShowError();
				}
			}
		} else if (KeyCode == 36 && CellIsEmpty == 0) {
			KeyCode = 0;
			unsigned char CellsData [CELL_CAPACITY];
			for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
				CellsData[i] = 0xFF;
			}
			waitInterrupt();
			WriteArrayToEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
			waitInterrupt();
			ReadCell(cell, CellsData, &CellIsEmpty);
		}


		setPoint(1, !CellIsEmpty);

		refreshInd();
	}
	PowerBlocked--;
}

void Run_getTemp() {

	getTemp_flags.Init = 1;
	getTemp_flags.Send_Address = 1;
	getTemp_flags.CountAddressBytes = 0;
	getTemp_flags.SendConvertTemp = 1;
	getTemp_flags.PauseValue = 120;
	getTemp_flags.SendGetTemp = 1;
	getTemp_flags.ReadData = 1;
	getTemp_flags.CountDataBytes = 0;
	getTemp_flags.Error = 0;

	getTemp_flags.ActiveProcess = 1;

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

	waitInterrupt();

	INIT();
	waitInterrupt();
	TX(0xCC);
	waitInterrupt();
	TX(0x44);

	unsigned char address = FindCell(END_OF_CELLS, 0);
	Broadcasting = address == END_OF_CELLS;

	unsigned char cell = 0;

	if (!Broadcasting) {
		FillArrayFromEEPROM(DS_Address, address, CELL_CAPACITY);
		cell = address / CELL_CAPACITY;
	}

	unsigned int data_on_ind_delay = 0;

	Run_getTemp();
	
	while (1) {

		if (KeyCode == 33) {
			KeyCode = 0;
			if (PowerBlocked != 1) {
				PowerBlocked = 1;
				setPoint(1, 1);
				data_on_ind_delay = 10000;
				refreshInd();
			} else {
				PowerBlocked = 0;
			}
		} else if (!Broadcasting && KeyCode == 31 || KeyCode == 32 || KeyCode == 34) {
			Reset_powerOnInterval();
			getTemp_flags.ActiveProcess = 0;
			address = FindCell((KeyCode == 34 ? END_OF_CELLS : cell * CELL_CAPACITY), (KeyCode == 31 ? 1 : 0));
			KeyCode = 0;

			waitInterrupt();
			FillArrayFromEEPROM(DS_Address, address, CELL_CAPACITY);
			cell = address / CELL_CAPACITY;

			CellToInd(cell);
			data_on_ind_delay = 18000;
			
			Run_getTemp();

		} else if (KeyCode == 36) {
			KeyCode = 0;
			EditAddressMemory();
			
			Reset_powerOnInterval();
			address = FindCell(END_OF_CELLS, 0);
			Broadcasting = address == END_OF_CELLS;
			if (Broadcasting) {
				setDigit(1, 32);
				setDigit(2, 32);
				setDigit(3, 32);
				refreshInd();
			} else {
				FillArrayFromEEPROM(DS_Address, address, CELL_CAPACITY);
				cell = address / CELL_CAPACITY;

				CellToInd(cell);
				data_on_ind_delay = 18000;
				Run_getTemp();
			}
		}

		if (getTemp_flags.Error) {
			ShowError();
		} else if (!getTemp_flags.ActiveProcess) {
			clrInd();
			indData();
		}

		if (data_on_ind_delay == 0) {
			refreshInd();
		} else {
			data_on_ind_delay--;
		}

		if (!getTemp_flags.ActiveProcess) {
			Run_getTemp();
		}


	}
}
