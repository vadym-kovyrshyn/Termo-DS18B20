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
#pragma config MCLRE = OFF      // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is digital input, MCLR internally tied to VDD)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = ON          // Flash Program Memory Code Protection bit (0000h to 07FFh code-protected)

__EEPROM_DATA(0x28, 0xFF, 0x13, 0xE7, 0x63, 0x15, 0x02, 0x5B); // 11
__EEPROM_DATA(0x28, 0xFF, 0x29, 0x89, 0x63, 0x15, 0x02, 0xE7); // 7
__EEPROM_DATA(0x28, 0xFF, 0xF8, 0xE7, 0x63, 0x15, 0x02, 0xF1); // 3
__EEPROM_DATA(0x28, 0xFF, 0xBE, 0xAC, 0x64, 0x15, 0x01, 0x68); // 2
__EEPROM_DATA(0x28, 0xFF, 0xEC, 0x95, 0x63, 0x15, 0x02, 0x3D); // 4
__EEPROM_DATA(0x28, 0xFF, 0x00, 0x93, 0x63, 0x15, 0x02, 0xCF); // 5
__EEPROM_DATA(0x28, 0xFF, 0x1D, 0xA8, 0x63, 0x15, 0x02, 0x83); // 6
__EEPROM_DATA(0x28, 0xFF, 0x2A, 0xA8, 0x63, 0x15, 0x02, 0x56); // 8
__EEPROM_DATA(0x28, 0xFF, 0xA5, 0xD4, 0x63, 0x15, 0x02, 0x48); // 9
__EEPROM_DATA(0x28, 0xFF, 0x65, 0xD3, 0x63, 0x15, 0x02, 0xEC); // 10
__EEPROM_DATA(0x28, 0xFF, 0x41, 0xA7, 0x63, 0x15, 0x02, 0xAD); // 12
__EEPROM_DATA(0x28, 0xFF, 0xA2, 0xD1, 0x64, 0x15, 0x01, 0x00); // 1

volatile unsigned char digits [3];
volatile unsigned char digits_0 [3];
volatile unsigned char KeyCode;
const unsigned char TMR0_VALUE = 235;
unsigned char digitemp;
int powerOnInterval;
const unsigned char PortAData[3] = {
	0b10000000,
	0b01000000,
	0b00000001,
};
bit endInterrupt;
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
	unsigned DataIsRead : 1;
	unsigned Error : 1;
	unsigned ActiveProcess : 1;
	unsigned Line : 8;
} getTemp_flags;

unsigned char DS_Address [] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char DS_ReadData [] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char temperature = 0; //температура
unsigned char temp_drob = 0; //дробная часть температуры
unsigned char sign; //знак температуры

#define key1 RA5
#define key2 RA2

#define STATE_REG TRISA
#define PIN_REG PORTA

#define FIRST_LINE 0b00001000
#define SECOND_LINE 0b00000010

#define CELLS_COUNT 16
#define CELL_CAPACITY (sizeof(DS_Address))
#define END_OF_CELLS (CELL_CAPACITY * CELLS_COUNT)
#define LAST_CELL (END_OF_CELLS - CELL_CAPACITY)

#define EMPTY_SYMBOL_VALUE 34

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
	digits[0] = digits_0[0];
	digits[1] = digits_0[1];
	digits[2] = digits_0[2];
}

void clrInd() {
	digits_0[0] = EMPTY_SYMBOL_VALUE;
	digits_0[1] = EMPTY_SYMBOL_VALUE;
	digits_0[2] = EMPTY_SYMBOL_VALUE;
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
			/*
			case 10: return 0b11111001; //A
			case 11: return 0b01101011; //B
			case 12: return 0b11000011; //C
			case 13: return 0b00111011; //D
			 */
		case 14: return 0b11100011; //E
			/*
			case 15: return 0b11100001; //F
			case 16: return 0b11111000; //G
			case 17: return 0b01101001; //H
			case 18: return 0b01000001; //I
			case 19: return 0b00011010; //J
			case 20: return 0b01000011; //L
			case 21: return 0b00101001; //N
			case 22: return 0b00101011; //O
			case 23: return 0b11110001; //P
			 */
		case 24: return 0b00100001; //R
			/*
			case 25: return 0b01101010; //S
			case 26: return 0b01100011; //T
			case 27: return 0b01011011; //U
			case 28: return 0b00001011; //V
			case 29: return 0b01110001; //Y
			case 30: return 0b10110001; //Z
			case 31: return 0b11110000; //°
			case 33: return 0b00000010; //_
			 */
		case 32: return 0b00100000; //–
			//	case EMPTY_SYMBOL_VALUE: return 0b00000000; //empty
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
unsigned char INIT(unsigned char line) {
	unsigned char One = line;
	unsigned char Zero = One ^ 0b11111111;

	unsigned char b;
	b = 0;
	STATE_REG |= One;
	__delay_us(20);
	STATE_REG &= Zero;
	__delay_us(500);
	STATE_REG |= One;
	__delay_us(65);
	b = (PIN_REG & One) > 0;
	__delay_us(450);

	return !b;
}

void TX(unsigned char cmd, unsigned char line) {
	unsigned char One = line;
	unsigned char Zero = One ^ 0b11111111;

	unsigned char temp = 0;
	unsigned char i = 0;
	temp = cmd;
	for (i = 0; i < 8; i++) {
		if (temp & 0x01) {
			STATE_REG &= Zero;
			__delay_us(5);
			STATE_REG |= One;
			__delay_us(70);
		} else {
			STATE_REG &= Zero;
			__delay_us(70);
			STATE_REG |= One;
			__delay_us(5);
		}
		temp >>= 1;
	}
}

unsigned char RX(unsigned char line) {
	unsigned char One = line;
	unsigned char Zero = One ^ 0b11111111;

	unsigned char d = 0;
	for (unsigned char i = 0; i < 8; i++) {
		STATE_REG &= Zero;
		__delay_us(6);
		STATE_REG |= One;
		__delay_us(4);
		d >>= 1;
		if ((PIN_REG & One) > 0) {
			d |= 0x80;
		}
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

/* Получаем значение температуры */
void get_temp_Async() {

	if (!getTemp_flags.ActiveProcess) {
		return;
	}

	unsigned char line = getTemp_flags.Line;

	// Step 1, 5
	if (getTemp_flags.Init) {
		if (INIT(line)) {
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
		if (Broadcasting) { //|| getTemp_flags.SendConvertTemp
			TX(0xCC, line);
			getTemp_flags.CountAddressBytes = 1;
			getTemp_flags.Send_Address = 0;
		} else if (getTemp_flags.CountAddressBytes < sizeof (DS_Address)) {
			if (getTemp_flags.CountAddressBytes == 0) {
				TX(0x55, line);
			}
			TX(DS_Address[getTemp_flags.CountAddressBytes], line);
			getTemp_flags.CountAddressBytes++;

			if (getTemp_flags.CountAddressBytes == sizeof (DS_Address)) {
				getTemp_flags.Send_Address = 0;
			}
		}
	} else
		// Step 3
		if (getTemp_flags.SendConvertTemp) {
		TX(0x44, line);
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
		TX(0xBE, line);
		getTemp_flags.SendGetTemp = 0;
	} else
		// Step 8
		if (getTemp_flags.ReadData) {
		if (getTemp_flags.CountDataBytes < sizeof (DS_ReadData)) {
			for (unsigned char i = 0; i < 3 && getTemp_flags.CountDataBytes < sizeof (DS_ReadData); i++) {
				DS_ReadData[getTemp_flags.CountDataBytes] = RX(line);
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
				getTemp_flags.DataIsRead = 1;
			}
		}
	} else {
		getTemp_flags.ActiveProcess = 0;
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
		static unsigned char DigitNumber = 0;

		T0IF = 0;
		TMR0 += TMR0_VALUE;

		if (DigitNumber > 2) {
			DigitNumber = 0;
		}
		unsigned char dig = digits[DigitNumber];
		digitemp = convDig(0b00111111 & dig);
		/*
			if (0b10000000==(0b10000000&dig)){
				if(!blink) digitemp = 0;
			}
		 */
		(0b01000000 == (0b01000000 & dig)) ? digitemp |= 0b00000100 : digitemp &= 0b11111011;

		PORTB = 0;
		PORTA = (PORTA & 0b00110100) | PortAData[DigitNumber++];

		PORTB = digitemp;

		endInterrupt = 1;
		if (powerOnInterval == 0) {
			if (PowerBlocked == 0) {
				TRISA4 = 1;
			}
		} else {
			powerOnInterval--;
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
	} else if (temperature < 10) {
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
		if (temp_drob > 9 && dN == 1) temp_drob /= 10;

		unsigned char v = (temp_drob > 9 ? 1 : 0);
		setDigit(cd - v, temp_drob % 10);
	}
}

unsigned char ReadCell(unsigned char cell, unsigned char * CellsData) {
	FillArrayFromEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
	for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
		if (CellsData[i] != 0xFF) {
			return 0;
		}
	}
	return 1;
}

void CellToInd(unsigned char cell) {
	clrInd();
	cell++;

	if (cell > 9) {
		setDigit(3, cell / 10);
		setDigit(2, cell % 10);
	} else {
		setDigit(3, cell);
	}

	refreshInd();
}

void EditAddressMemory() {
	getTemp_flags.ActiveProcess = 0;

	PowerBlocked++;

	clrInd();

	unsigned char cell = 0;
	unsigned char CellIsEmpty = 1;
	unsigned char CellsData [CELL_CAPACITY];

	unsigned char ErrorShowed = 0;
	unsigned char RereadCell = 1;
	while (1) {

		if (KeyCode != 0 && ErrorShowed) {
			KeyCode = 0;
			ErrorShowed = 0;
			RereadCell = 1;
		}

		if (KeyCode == 31) {
			KeyCode = 0;
			if (cell > 0) {
				cell--;
			} else {
				cell = CELLS_COUNT - 1;
			}
			RereadCell = 1;
		} else if (KeyCode == 32) {
			KeyCode = 0;
			if (cell < CELLS_COUNT - 1) {
				cell++;
			} else {
				cell = 0;
			}
			RereadCell = 1;
		} else if (KeyCode == 34) {
			KeyCode = 0;
			clrInd();
			refreshInd();
			break;
		} else if (KeyCode == 35 && CellIsEmpty == 1) {
			KeyCode = 0;
			waitInterrupt();
			if (INIT(SECOND_LINE)) {
				waitInterrupt();
				TX(0x33, SECOND_LINE);
				for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
					waitInterrupt();
					CellsData[i] = RX(SECOND_LINE);
				}
				if (CellsData[CELL_CAPACITY - 1] == calc_crc(CellsData, CELL_CAPACITY - 1)) {
					waitInterrupt();
					WriteArrayToEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
					waitInterrupt();
					RereadCell = 1;
				} else {
					ShowError();
					ErrorShowed = 1;
				}
			} else {
				ShowError();
				ErrorShowed = 1;
			}
		} else if (KeyCode == 36 && CellIsEmpty == 0) {
			KeyCode = 0;
			for (unsigned char i = 0; i < CELL_CAPACITY; i++) {
				CellsData[i] = 0xFF;
			}
			waitInterrupt();
			WriteArrayToEEPROM(CellsData, cell * CELL_CAPACITY, CELL_CAPACITY);
			waitInterrupt();
			RereadCell = 1;
		}

		if (RereadCell) {
			RereadCell = 0;
			CellIsEmpty = ReadCell(cell, CellsData);
			CellToInd(cell);
			setPoint(1, !CellIsEmpty);
			refreshInd();
		}
	}
	waitInterrupt();
	PowerBlocked--;
}

void Run_getTemp(unsigned char line) {

	getTemp_flags.Init = 1;
	getTemp_flags.Send_Address = 1;
	getTemp_flags.CountAddressBytes = 0;
	getTemp_flags.SendConvertTemp = 1;
	getTemp_flags.PauseValue = 120;
	getTemp_flags.SendGetTemp = 1;
	getTemp_flags.ReadData = 1;
	getTemp_flags.CountDataBytes = 0;
	getTemp_flags.Error = 0;
	getTemp_flags.DataIsRead = 0;
	getTemp_flags.Line = line;

	getTemp_flags.ActiveProcess = 1;

}

void Run_getInit(unsigned char line) {

	getTemp_flags.Init = 1;
	getTemp_flags.Send_Address = 0;
	getTemp_flags.CountAddressBytes = 0;
	getTemp_flags.SendConvertTemp = 0;
	getTemp_flags.PauseValue = 0;
	getTemp_flags.SendGetTemp = 0;
	getTemp_flags.ReadData = 0;
	getTemp_flags.CountDataBytes = 0;
	getTemp_flags.Error = 0;
	getTemp_flags.DataIsRead = 0;
	getTemp_flags.Line = line;

	getTemp_flags.ActiveProcess = 1;

}

void main() {

	INTCON = 0;
	OPTION_REG = 0b00000111;
	TRISA = 0b00101110;
	TRISB = 0b00000000;
	PORTA = 0b00000000;
	PORTB = 0b00000000;
	TMR0 = TMR0_VALUE;
	T2CON = 0b00000100;
	CMCON = 0b00000111;

	clrInd();
	refreshInd();

	INTCON = 0b10100000;

	Reset_powerOnInterval();

	unsigned char cell = 0;
	unsigned char address;
	unsigned int point_on_ind_delay = 0;
	unsigned char TheStart = 1;
	unsigned char line = FIRST_LINE;

	KeyCode = 36;

	while (1) {

		if (KeyCode == 33) {
			KeyCode = 0;
			if (PowerBlocked != 1) {
				PowerBlocked = 1;
				point_on_ind_delay = 4000;
			} else {
				powerOnInterval = 0;
				PowerBlocked = 0;
			}
		} else if (KeyCode == 31 || KeyCode == 32 || KeyCode == 34) {
			Reset_powerOnInterval();

			if (Broadcasting && KeyCode != 34) {

				if (KeyCode == 31 && line != FIRST_LINE) {
					TheStart = 1;
					line = FIRST_LINE;
				} else if (KeyCode == 32 && line != SECOND_LINE) {
					TheStart = 1;
					line = SECOND_LINE;
				} else {
					continue;
				}
				KeyCode = 0;
				getTemp_flags.ActiveProcess = 0;
				waitInterrupt();
				Run_getTemp(line);

			} else {
				getTemp_flags.ActiveProcess = 0;

				address = FindCell((KeyCode == 34 ? END_OF_CELLS : cell * CELL_CAPACITY), (KeyCode == 31 ? 1 : 0));
				Broadcasting = address == END_OF_CELLS;
				KeyCode = 0;
				if (Broadcasting) {
					TheStart = 1;
				} else {
					FillArrayFromEEPROM(DS_Address, address, CELL_CAPACITY);
					cell = address / CELL_CAPACITY;
					CellToInd(cell);
				}
				line = FIRST_LINE;
				Run_getInit(line);
				while(getTemp_flags.ActiveProcess);
				Run_getTemp(line);
			}

		} else if (KeyCode == 35) {
			Reset_powerOnInterval();
			KeyCode = 0;
			Broadcasting = 1;
			line = SECOND_LINE;
			getTemp_flags.ActiveProcess = 0;
			waitInterrupt();
			Run_getTemp(line);
			TheStart = 1;
		} else if (KeyCode == 36) {
			KeyCode = 0;
			if (!TheStart) {
				EditAddressMemory();
			}

			Reset_powerOnInterval();
			address = FindCell(END_OF_CELLS, 0);
			Broadcasting = address == END_OF_CELLS;
			if (Broadcasting) {
				TheStart = 1;
			} else {
				FillArrayFromEEPROM(DS_Address, address, CELL_CAPACITY);
				cell = address / CELL_CAPACITY;

				CellToInd(cell);
			}
			line = FIRST_LINE;
			Run_getTemp(line);
		}

		if (getTemp_flags.Error) {
			if (Broadcasting) {
				if (line == SECOND_LINE) {
					line = FIRST_LINE;
				}
			} else {
				clrInd();
				ShowError();
			}
			Run_getTemp(line);
		} else if (getTemp_flags.DataIsRead) {
			clrInd();
			indData();
			Run_getTemp(line);
		} else if (TheStart) {
			TheStart = 0;
			clrInd();
			setDigit(1, 32);
			setDigit(2, 32);
			setDigit(3, 32);
		}

		if (point_on_ind_delay > 0) {
			setPoint(1, 1);
			point_on_ind_delay--;
		}

		refreshInd();

	}
}
