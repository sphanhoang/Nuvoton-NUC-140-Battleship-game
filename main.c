//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"
#include "LCD.h"
#include "Draw2D.h"

#define SYSTICK_DLAY_us 30000
#define TIMER1_COUNTS 5000 - 1; // (TCMPR +1) * (1/1MHz) = 0.005s = time to display each 7-seg LED

#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2


//------------------------------------------------------------------------------------------------------------------------------------
// Functions declaration
//------------------------------------------------------------------------------------------------------------------------------------
void System_Config(void);
void SPI3_Config(void);

void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);

void UART0_Config(void);
void TIMER1_Config(void);

void sevenSeg_off(void);
void KeyPadEnable(_Bool a);
uint8_t KeyPadScanning(void);

void mapDraw(void); // draw map
void mapReset(void); // reset map for new game 

void numDisplay(int x_val, int y_val, int shootCount_val); // 7-seg display function declare

// Interrupt flags:
volatile _Bool GP15_isPressed = FALSE,
							 K9_isPressed = FALSE,
							 mapStatus = FALSE;
							 win = FALSE;
// Variables:
volatile int row, 
						 column, 
						 tiles,
						 shootCount = 0, // count shots
						 x = 1, y = 1, // x and y coordinate
						 timeCount = 0, // variable for 7-seg LEDs
						 choose = 1,  // variable to switch case in main
						 score = 0, // keep trakc current score
						 max_score; // total score that player can score.
// map:
volatile char ship_Placement[8][8] = {{0,0,0,0,0,0,0,0},
																			{0,1,1,0,0,0,0,0},
																			{0,0,0,0,0,0,0,0},
																			{0,0,0,0,0,0,0,0},
																			{0,0,1,1,0,0,0,0},
																			{0,0,0,0,0,0,0,0},
																			{1,1,0,0,0,0,0,0},
																			{0,0,0,0,0,0,0,0}},
							shoot[8][8]; // shoot record map

												

int digit[4]; // digit array stores value to show on 7-seg LED display
char score_val[2] = "00";
int pattern[] = {
               //   gedbaf_dot_c
                  0b10000010,  //Number 0          // ---a----
                  0b11101110,  //Number 1          // |      |
                  0b00000111,  //Number 2          // f      b
                  0b01000110,  //Number 3          // |      |
                  0b01101010,  //Number 4          // ---g----
                  0b01010010,  //Number 5          // |      |
                  0b00010010,  //Number 6          // e      c
                  0b11100110,  //Number 7          // |      |
                  0b00000010,  //Number 8          // ---d----
                  0b01000010,  //Number 9
                  0b01111111   //Blank LED 
                };    		

								
//------------------------------------------------------------------------------------------------------------------------------------
// Main program
//------------------------------------------------------------------------------------------------------------------------------------

typedef enum {welcome_screen, main_game, end_game} STATES;

int main(void)
{	
	STATES game_state;
	//--------------------------------
	//System initialization
	//--------------------------------
	System_Config();
	UART0_Config();

	//--------------------------------
	//SPI3 initialization
	//--------------------------------
	SPI3_Config();

	//--------------------------------
	//LCD initialization
	//--------------------------------    
	LCD_start();
	LCD_clear();

	//--------------------------------
	//LCD static content
	//--------------------------------    

	//--------------------------------
	//LCD dynamic content
	//-------------------------------- 
	game_state = welcome_screen;

	
	while (1) {
			switch (game_state){
				case welcome_screen:				
						printS_5x7(12,16, "Welcome to BATTLESHIP!");
						printS_5x7(12,32, "Press GP15 to play!");
						
						while(!(GP15_isPressed == TRUE)); // wait for user to press GP15 button
						GP15_isPressed = FALSE; // clear the flag for GP15 button input
				
						LCD_clear();
						KeyPadEnable(TRUE); // Enable the keypad
						mapReset();
						mapDraw(); //Draw map
				
						// count total ship parts on the map:
						for (int i = 0; i < 8; i++){
							for (int j = 0; j < 8; j++){
								max_score = max_score + ship_Placement[j][i]; 
							}
						}
						
						game_state = main_game; //switch to main game
						break;	
				case main_game:
						// print score:
						sprintf(score_val, "%i", score);
						printS_5x7(128 - 5*6, 16, "Score");
						printS_5x7(128 - 5*4, 32, score_val);
						// K9 flag:
						K9_isPressed = FALSE; // reset k9 flag
						if(KeyPadScanning() == 9){ 
								K9_isPressed = TRUE; // K9_isPressed is true when K9 key has been preassed
						}
						// main game steps:
						switch (choose) {
							case 1:
								if (K9_isPressed == TRUE) {choose = 2;} // scan if K9 has been pressed, if yes --> switch to case 2
								if (GP15_isPressed == TRUE) {choose = 3;} // scan if GP5 has been pressed, if yes --> switch to case 3
								if (KeyPadScanning() != 0 && KeyPadScanning() != 9){ // If Key pad 1 to 8 is pressed
									x = KeyPadScanning(); // assign pressed key index to 'x' varibale
								}
								break;
							case 2:
								if (K9_isPressed == TRUE) {choose = 1;} // scan if K9 has been pressed, if yes --> switch to case 2
								if (GP15_isPressed == TRUE) {choose = 3;} // scan if K9 has been pressed, if yes --> switch to case 3
								if (KeyPadScanning() != 0 && KeyPadScanning() != 9){ // If Key pad 1 to 8 is pressed
									y = KeyPadScanning(); // assign pressed key index to 'y' varibale
								}
								break;
							case 3:
								GP15_isPressed = FALSE; // clear the flag for GP15 button input
								shootCount++; // shoot count + 1
							
								// check if there is ship at current x, y coordinate where player haven't shot at
								if (ship_Placement[y-1][x-1] == 1 && shoot[y-1][x-1] != 2){
										shoot[y-1][x-1] = 2; // mark direct hit by setting value = 2
										score++; // score + 1 on hit
										for(int i = 0; i < 6; i++){
											sevenSeg_off();
											PC->DOUT ^= (1<<12);	// Blink LED5 three times
											CLK_SysTickDelay(SYSTICK_DLAY_us);
										}
								// check if miss
								} 	else if (ship_Placement[y-1][x-1] == 0 && shoot[y-1][x-1] != 1){
										// mark shoot location by changing value from 0 to 1
										shoot[y-1][x-1] = 1;
								}	 
								mapDraw(); // update map
								choose = 1; // return immediately by default to case 1 for x coordinate input.
								break;
							default: break;
						}							
												
						numDisplay(x, y, shootCount); // display x, y and shoot count on 7seg-LEDs
				
						if (shootCount > 16) {win = FALSE; LCD_clear(); game_state = end_game;}
						if (score == max_score) {win = TRUE; LCD_clear(); game_state = end_game;}
						
						break;
				case end_game:
						sevenSeg_off(); // turn off 7-seg LED
						KeyPadEnable(FALSE); // disable the keypad
						if(win == TRUE){
							printS_5x7(12,16, "YOU WON!");
						} else if (win == FALSE){
							printS_5x7(12,16, "YOU LOST!");
						}
						printS_5x7(12,32, "Press GP15");
						printS_5x7(12,48, "to play again!");
						for(int i = 0; i < 6; i++){
										PB->DOUT ^= (1<<11);	// toggle buzzer
										CLK_SysTickDelay(SYSTICK_DLAY_us);
									}
						while(!(GP15_isPressed == TRUE)); // wait for user to press GP15 button
						GP15_isPressed = FALSE; // clear the flag for GP15 button input
						x = y = 1;
						shootCount = score = 0;  // reset variables
						LCD_clear();
						game_state = welcome_screen;
						break;
				default: break;
			}
		
	}
}

//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------

void LCD_start(void)
{
	LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps  
	LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)  
	LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()        	
	LCD_command(0xC0);
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
	SPI3->SSR |= 1ul << 0;
	SPI3->TX[0] = temp;
	SPI3->CNTRL |= 1ul << 0;
	while (SPI3->CNTRL & (1ul << 0));
	SPI3->SSR &= ~(1ul << 0);
}

void LCD_data(unsigned char temp)
{
	SPI3->SSR |= 1ul << 0;
	SPI3->TX[0] = 0x0100 + temp;
	SPI3->CNTRL |= 1ul << 0;
	while (SPI3->CNTRL & (1ul << 0));
	SPI3->SSR &= ~(1ul << 0);
}

void LCD_clear(void)
{
	int16_t i;
	LCD_SetAddress(0x0, 0x0);
	for (i = 0; i < 132 * 8; i++)
	{
		LCD_data(0x00);
	}
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
	LCD_command(0xB0 | PageAddr);
	LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
	LCD_command(0x00 | (ColumnAddr & 0xF));
}

void KeyPadEnable(_Bool a) {
	if (a == TRUE){
		GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
		GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
		GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
		GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
		GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
		GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
	} else {
	}
	
}

uint8_t KeyPadScanning(void) {
	PA0 = 1; PA1 = 1; PA2 = 0; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 1;
	if (PA4 == 0) return 4;
	if (PA5 == 0) return 7;
	PA0 = 1; PA1 = 0; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 2;
	if (PA4 == 0) return 5;
	if (PA5 == 0) return 8;
	PA0 = 0; PA1 = 1; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 3;
	if (PA4 == 0) return 6;
	if (PA5 == 0) return 9;
	return 0;
}

//Timer1 Config starts
void TIMER1_Config(void) 
{
	CLK->CLKSEL1 &= ~(0b111 << 12); // 12MHz
	CLK->APBCLK |= (1 << 3);
	TIMER1->TCSR &= ~(0xFF << 0);

	//reset Timer 1
	TIMER1->TCSR |= (1 << 26);
	
	//set the prescale;
	TIMER1->TCSR |= 11; //TIMER1_PRESCALE = 11, FOUT = 12Mhz/(11+1) = 1 Mhz

	//define Timer 1 operation mode
	TIMER1->TCSR &= ~(0b11 << 27);
	TIMER1->TCSR |= (0b01 << 27); //periodic mode 
	TIMER1->TCSR &= ~(1 << 24);	   

	//TDR to be updated continuously while timer counter is counting
	TIMER1->TCSR |= (1 << 16);
	//Enable TE bit (bit 29) of TCSR
	//The bit will enable the timer interrupt flag TIF
	TIMER1->TCSR |= (1 << 29);
	
	//TIMER1 interrupt
	NVIC->ISER[0] |= 1 << 9;
	NVIC->IP[0] &= (~(3 << 14)); // settting priority
	
	TIMER1->TCMPR = TIMER1_COUNTS; 
	TIMER1->TCSR |= (1 << 30);
	//Timer 1 initialization end----------------
}

//UART0 Config starts
void UART0_Config(void) {
	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0x03 << 2);
	PB->PMD |= (0x01 << 2); // PB.1 is output pin
	SYS->GPB_MFP |= (0x01 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin	
	PB->PMD &= ~(0b11 << 0);	// Set Pin Mode for GPB.0(RX - Input)

	// UART0 operation configuration
	UART0->FCR |= (0x03 << 1); // clear both TX & RX FIFO
	UART0->LCR &= ~(0x01 << 3); // no parity bit
	UART0->LCR &= ~(0x01 << 2); // one stop bit
	UART0->LCR |= (0x03 << 0); // 8 data bit

	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(70+2)]= 19200 bps
	UART0->BAUD &= ~(0x0FFFF << 0);
	UART0->BAUD |= 70;
	UART0->BAUD &= ~(0x03 << 28); // mode 0
	
	//UART 0 interrupt configuration
	UART0 -> IER  |= (1 << 0); // Receive Data Available Interrupt Enable
	UART0 -> FCR &= ~(0x0F << 16); // FIFO Trigger Level is 1 byte
	NVIC->ISER[0] |= 1 << 12; //
	NVIC->IP[3] &= ~(3 << 6); // Set priority of UART0 as 0, slowest
}


void System_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers
	CLK->PWRCON |= (0x01 << 0); 
	while (!(CLK->CLKSTATUS & (1 << 0)));
	
	// clock source selection
	CLK->CLKSEL0 &= (~(0b111 << 0)); // 12Mhz clock source
	CLK->CLKDIV &= ~(11 << 0); // HCLK_N = 11 -> HCLK = 12 Mhz/11+1 = 1 Mhz

	// enable clock of SPI3
	CLK->APBCLK |= 1 << 15;

	CLK->CLKSEL1 |= (0x03 << 24); // UART0 clock source is 22.1184 MHz
	CLK->CLKDIV &= ~(0x0F << 8); // clock divider is 1
	CLK->APBCLK |= (0x01 << 16); // enable UART0 clock
	
	TIMER1_Config(); // Setup timer 1
	
	PA->DBEN |= (0b111 << 3); // GPIO Port A Enable debouncing for key matrix
	PB->DBEN |= (0x01 << 15); // GPIO Port B De-bounce Enable
	
	GPIO -> DBNCECON &= (~(1<<4)); // Debounce clock source: HCLK = 1 Mhz
	GPIO -> DBNCECON |= 15; // Sample input once per 128 * 256 cycle ~ 0.03 sec
	
	// Enable the interrupt and set priority	
	NVIC->ISER[0] |= 1 << 3; // 
	NVIC->IP[0] &= (~(3 << 30)); // Setting priority
	
	// Pin configuration
	GPIO_SetMode(PC, BIT12, GPIO_MODE_OUTPUT);	  // LED
	GPIO_SetMode(PC, BIT15, GPIO_MODE_OUTPUT);		// Button
	GPIO_SetMode(PB, BIT11, GPIO_MODE_OUTPUT); 		// Buzzer
	
	// Set mode for PC4 to PC7 for Control Pins
	PC->PMD &= (~(0xFF<< 8));		//Clear PMD[15:8] 
	PC->PMD |= (0b01010101 << 8);  //Set output push-pull for PC4 to PC7 
	
	// Set mode for PE0 to PE7
	PE->PMD &= (~(0xFFFF<< 0));		//Clear PMD[15:0] 
	PE->PMD |= 0b0101010101010101<<0;   //Set output push-pull for PE0 to PE7
	

	GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT); // SW_INT(GP15) button as input
	PB->IMD &= (~(1 << 15));// Setting edge trigger
	PB->IEN |= (1 << 15); // Setting falling edge trigger
	PB->DBEN |= 1 << 15; // Enable button debounce
	SYS_LockReg();  // Lock protected registers  
}

void SPI3_Config(void) {
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function

	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt    
	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high 
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first   
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer

	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK       
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}

// 7-seg enable:
void sevenSeg_off(void){
	PC->DOUT &= ~(1<<7);    //Turn off U11 7-seg LED
	PC->DOUT &= ~(1<<6);		//Turn off U12
	PC->DOUT &= ~(1<<5);		//---------U13
	PC->DOUT &= ~(1<<4);		//---------U14
}
// 7-seg led control:
void numDisplay(int x_val, int y_val, int shootCount_val) {
	digit[0] = x_val;	// first digit stores x coordinate
	digit[1] = y_val;	// second digit stores y coordinate
	digit[2] = shootCount_val / 10; // third digit stores tens of shootCount value
	digit[3] = shootCount_val % 10;	// fourth digit stores unit of shootCount value

	sevenSeg_off();
			
	PC->DOUT |= (1<<7-timeCount);		// display current LED
	PE->DOUT = pattern[digit[timeCount]]; // display corresponding digit
	
		
	}


void mapDraw(void) {
	for(int i = 0; i < 8; i++){ // column
		for(int j = 0; j < 8; j++){ // row
			if (shoot[j][i] == 2){
				printC_5x7(i*10 + 15, j*7 + 7, 'X'); // Draw X on hit
			} else if (shoot[j][i] == 1){
				printC_5x7(i*10 + 15, j*7 + 7, ' '); // Leave blank on miss
			}	else {
				printC_5x7(i*10 + 15, j*7 + 7, '-'); // Draw water
			}
		}
	}
}

void mapReset(void) {
	for(int i = 0; i < 8; i++){ // column
		for(int j = 0; j < 8; j++){ // row
			shoot[j][i] = '0'; // Reset shoot map
		}
	}
}

// GP15 ISR
void EINT1_IRQHandler(void) {
	GP15_isPressed = TRUE;
//	GP15_isPressed = FALSE; // clear the flag for GP15 button input
	PB->ISRC |= (1 << 15);
//	CLK_SysTickDelay(SYSTICK_DLAY_us);
}

// Timer1 ISR
void TMR1_IRQHandler(void) {
	timeCount++;
	if (timeCount > 3){timeCount = 0;}
	TIMER1->TISR |= 1<<0;
}

// UART0 ISR
void UART02_IRQHandler(void)
{
	if (mapStatus == TRUE){ return; } // If map load is TRUE, return the whole loaded in data of the map
	
	char ch = UART0->DATA;

	if (ch == 13){ return; } // Carriage return
	if (ch == 10){ row++; } // Line feed
	
	
	
	if (ch == '0' || ch == '1') // If the map data is 0 or 1
	{
		ship_Placement[row][column] = ch;
		column++;
		tiles	++;
	}	
	if (column >= 8) 
		column = 0;
	
	if (tiles == 64) // If loaded all the maps (8x8 = 64)
	{
		LCD_clear();
		printS_5x7(5, 24 , "Map Loaded Successfully!");
		tiles = 0;
		row = 0;
		column = 0;
		mapStatus = TRUE;
	}
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
