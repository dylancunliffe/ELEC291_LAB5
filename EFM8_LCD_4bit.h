#define SYSCLK      72000000L  // SYSCLK frequency in Hz
#define BAUDRATE      115200L  // Baud rate of UART in bps
#define LCD_RS P1_7
// #define LCD_RW Px_x // Not used in this code.  Connect to GND
#define LCD_E1 P2_0
#define LCD_E2 P2_1
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define CHARS_PER_LINE 16
#define LED_green_pin P2_5
#define LED_red_pin P2_6
#define button_pin P3_0

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

void LCD1_pulse (void)
{
	LCD_E1=1;
	Timer3us(40);
	LCD_E1=0;
}
void LCD2_pulse (void)
{
	LCD_E2=1;	
	Timer3us(40);
	LCD_E2=0;
}

void LCD_byte (unsigned char x, bit LCD_select)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	if(LCD_select == 0)
	{
		LCD1_pulse();
	}
	else
	{
		LCD2_pulse();
	}
	Timer3us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	if(LCD_select == 0)
	{
		LCD1_pulse();
	}
	else
	{
		LCD2_pulse();
	}
}

void WriteData (unsigned char x, bit LCD_select)
{
	LCD_RS=1;
	LCD_byte(x, LCD_select);
	waitms(2);
}

void WriteCommand (unsigned char x, bit LCD_select)
{
	LCD_RS=0;
	LCD_byte(x, LCD_select);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E1=0; // Resting state of LCD's enable is zero
	LCD_E2=0;

	// LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33, 0);
	WriteCommand(0x33, 0);
	WriteCommand(0x32, 0); // Change to 4-bit mode
	waitms(20);
	WriteCommand(0x33, 1);
	WriteCommand(0x33, 1);
	WriteCommand(0x32, 1); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28, 0);
	WriteCommand(0x0c, 0);
	WriteCommand(0x01, 0); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
	WriteCommand(0x28, 1);
	WriteCommand(0x0c, 1);
	WriteCommand(0x01, 1); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, bit LCD_select, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80, LCD_select); // Set the cursor to the beginning of the line
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j], LCD_select);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' ', LCD_select); // Clear the rest of the line
}

int getsn (char * buff, int len)
{
	int j;
	char c;
	
	for(j=0; j<(len-1); j++)
	{
		c=getchar();
		if ( (c=='\n') || (c=='\r') )
		{
			buff[j]=0;
			return j;
		}
		else
		{
			buff[j]=c;
		}
	}
	buff[j]=0;
	return len;
}

/*
void main (void)
{
	char buff[17];
	int line;
	int lcd;
	// Configure the LCD
	LCD_4BIT();
	
   	// Display something in the LCD
	LCDprint("Print w/ Putty", 1, 1, 0);
	while(1)
	{
		printf("What LCD do you want to write to? (1 or 2): ");
		getsn(buff, sizeof(buff));
		lcd=(int)buff[0];
		printf("\n");
		printf("What line do you want to write to? (1 or 2): ");
		getsn(buff, sizeof(buff));
		line=(int)buff[0];
		printf("\n");
		printf("Type what you want to display in line %d (16 char max): ", line);
		getsn(buff, sizeof(buff));
		printf("\n");

		LCDprint(buff, line-48, 1, lcd-49);
		LCDprint(buff, line-48, 1, lcd-49);
	}
}
*/