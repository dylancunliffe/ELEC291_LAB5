// FreqEFM8.c: Measure the frequency of a signal on pin T0.
  
#include <EFM8LB1.h>
#include <stdio.h>
#include "EFM8_LCD_4bit.h"
#include <string.h>

#define SYSCLK      72000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps

unsigned long F;
float tolerance = 0;
float C;
float target = 0.0f;
float C_needed;

char buff[17];
char freq[16];

unsigned char overflow_count;
char connection;

bit valid1 = 0;
bit valid2 = 0;
bit valid3 = 0;
bit enter_target = 0;
bit target_active = 0;
	
char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN |= 0x80;
	RSTSRC = 0x02;

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X10; // Enable T0 on P0.0
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	// Configure Uart 0
	SCON0 = 0x10;
	CKCON0 |= 0b_0000_0000 ; // Timer 1 uses the system clock divided by 12.
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
	
	return 0;
}

void TIMER0_Init(void)
{
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0101; // Timer/Counter 0 used as a 16-bit counter
	TR0=0; // Stop Timer/Counter 0
}

void main (void) 
{
	int i = 0;
	LED_green_pin = 1;
	LED_red_pin = 1;
	TIMER0_Init();

	// Configure the LCD
	LCD_4BIT();

	waitms(500); // Give PuTTY a chance to start.
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	printf ("EFM8 Frequency measurement using Timer/Counter 0.\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);

	LCDprint("Press button", 1, 1, 1);
	LCDprint("to set target.", 1, 2, 1);


	while(1)
	{
		TL0=0;
		TH0=0;
		overflow_count=0;
		TF0=0;
		TR0=1; // Start Timer/Counter 0
		waitms(1000);
		TR0=0; // Stop Timer/Counter 0
		F=overflow_count*0x10000L+TH0*0x100L+TL0;
		C = 240000;
		C /= (float)F;
		C -= 6;
		if (C < 0) {
			C = 0;
			F = 99999;
		}

		//printf("\rc=%fnF\n", C);
		//printf("\x1b[0K"); // ANSI: Clear from cursor to end of line.
		//printf("\rf=%luHz\n", F);
		sprintf(buff, "Cap (nF): %.1f", C);
		LCDprint(buff, 0, 1, 1);

		if(F == 99999) {
			LCDprint("Freq (Hz): Error", 0, 2, 1);
		}
		else{
			sprintf(freq, "Freq (Hz): %lu", F);
			LCDprint(freq, 0, 2, 1);
		}

		enter_target = button_pin;
		if (enter_target == 1) {
			valid1 = 0;
			valid2 = 0;
			valid3 = 0;
			LED_green_pin = 1;
			LED_red_pin = 1;
			LCDprint("Enter target", 0, 1, 1);
			LCDprint("to see values.", 0, 2, 1);
			LCDprint("Use putty", 1, 1, 1);
			LCDprint("to set target.", 1, 2, 1);
			while(valid2 != 1){	
				printf("Enter target capacitance in nano Farads (nF): \n");
				target = 0;
				getsn(buff, sizeof(buff));

				for(i = 0; i < strlen(buff); i++){
					target = (target * 10.0f) + (buff[i] - '0');
				}

				printf("\n");

				if(target < 0 || target > 1000){
					printf("Error: Invalid Entry. Enter again. \n");
				}
				else valid2 = 1;
			}

			while(valid3 != 1) {
				printf("Enter target tolerance in percent (%): \n");
				tolerance = 0;
				getsn(buff, sizeof(buff));

				for(i = 0; i < strlen(buff); i++){
					tolerance = (tolerance * 10.0f) + (buff[i] - '0');
				}

				printf("\n");

				if(tolerance < 0 || tolerance > 100){
					printf("Error: Invalid Entry. Enter again. \n");
				}
				else valid3 = 1;
			}

			while(valid1 != 1) {
				printf("Parallel (P) or series (S) connection? (Enter P or S): \n");
				connection = '\0';
				getsn(buff, sizeof(buff));
				connection = (char)buff[0];
				printf("\n");

				if(connection != 'P' && connection != 'S' && connection != 'p' && connection != 's'){
					printf("Error: Invalid Entry. Enter again. \n");
				}
				else valid1 = 1;
			}
			enter_target = 0;
			target_active = 1;
		}

		if(target_active == 1){
			if (connection == 'p' || connection == 'P') {
    			C_needed = target - C;
				LCDprint("Parallel nF req.:", 1, 1, 1);
			}

			else if (connection == 's' || connection == 'S'){
    			C_needed = (float)(1 / ((1/target) - (1/C))); 
				LCDprint("Series nF req.:", 1, 1, 1);
			}
			else {
				LCDprint("Overflow", 1, 2, 1);
			}

			if((C < (target * (1 + (tolerance / 100)))) && (C > (target * (1 - (tolerance / 100))))) {
				LED_green_pin = 0;
				LED_red_pin = 1;
			}
			else {
				LED_green_pin = 1;
				LED_red_pin = 0;
			}

			if (LED_green_pin == 0) {
				LCDprint("Within tolerance", 1, 2, 1);
			}
			else if (C_needed < 9999 && C_needed > -9999) {
				sprintf(buff, "%.1f nF", C_needed);
				LCDprint(buff, 1, 2, 1);
			}
			else {
				LCDprint("Target too close", 1, 2, 1);
			}

		}
	}
}
