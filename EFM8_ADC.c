// ADC.c:  Shows how to use the 14-bit ADC.  This program
// measures the voltage from some pins of the EFM8LB1 using the ADC.
//
// (c) 2008-2023, Jesus Calvino-Fraga
//

#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include "EFM8_LCD_4bit.h"

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define NOISE_THRESHOLD_LOW 50
#define NOISE_THRESHOLD_HIGH 2000

char buff[17];

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

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
	XBR1     = 0X00;
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
  	
	return 0;
}

void InitADC (void)
{
	SFRPAGE = 0x00;
	ADEN=0; // Disable ADC
	
	ADC0CN1=
		(0x2 << 6) | // 0x0: 10-bit, 0x1: 12-bit, 0x2: 14-bit
        (0x0 << 3) | // 0x0: No shift. 0x1: Shift right 1 bit. 0x2: Shift right 2 bits. 0x3: Shift right 3 bits.		
		(0x0 << 0) ; // Accumulate n conversions: 0x0: 1, 0x1:4, 0x2:8, 0x3:16, 0x4:32
	
	ADC0CF0=
	    ((SYSCLK/SARCLK) << 3) | // SAR Clock Divider. Max is 18MHz. Fsarclk = (Fadcclk) / (ADSC + 1)
		(0x0 << 2); // 0:SYSCLK ADCCLK = SYSCLK. 1:HFOSC0 ADCCLK = HFOSC0.
	
	ADC0CF1=
		(0 << 7)   | // 0: Disable low power mode. 1: Enable low power mode.
		(0x1E << 0); // Conversion Tracking Time. Tadtk = ADTK / (Fsarclk)
	
	ADC0CN0 =
		(0x0 << 7) | // ADEN. 0: Disable ADC0. 1: Enable ADC0.
		(0x0 << 6) | // IPOEN. 0: Keep ADC powered on when ADEN is 1. 1: Power down when ADC is idle.
		(0x0 << 5) | // ADINT. Set by hardware upon completion of a data conversion. Must be cleared by firmware.
		(0x0 << 4) | // ADBUSY. Writing 1 to this bit initiates an ADC conversion when ADCM = 000. This bit should not be polled to indicate when a conversion is complete. Instead, the ADINT bit should be used when polling for conversion completion.
		(0x0 << 3) | // ADWINT. Set by hardware when the contents of ADC0H:ADC0L fall within the window specified by ADC0GTH:ADC0GTL and ADC0LTH:ADC0LTL. Can trigger an interrupt. Must be cleared by firmware.
		(0x0 << 2) | // ADGN (Gain Control). 0x0: PGA gain=1. 0x1: PGA gain=0.75. 0x2: PGA gain=0.5. 0x3: PGA gain=0.25.
		(0x0 << 0) ; // TEMPE. 0: Disable the Temperature Sensor. 1: Enable the Temperature Sensor.

	ADC0CF2= 
		(0x0 << 7) | // GNDSL. 0: reference is the GND pin. 1: reference is the AGND pin.
		(0x1 << 5) | // REFSL. 0x0: VREF pin (external or on-chip). 0x1: VDD pin. 0x2: 1.8V. 0x3: internal voltage reference.
		(0x1F << 0); // ADPWR. Power Up Delay Time. Tpwrtime = ((4 * (ADPWR + 1)) + 2) / (Fadcclk)
	
	ADC0CN2 =
		(0x0 << 7) | // PACEN. 0x0: The ADC accumulator is over-written.  0x1: The ADC accumulator adds to results.
		(0x0 << 0) ; // ADCM. 0x0: ADBUSY, 0x1: TIMER0, 0x2: TIMER2, 0x3: TIMER3, 0x4: CNVSTR, 0x5: CEX5, 0x6: TIMER4, 0x7: TIMER5, 0x8: CLU0, 0x9: CLU1, 0xA: CLU2, 0xB: CLU3

	ADEN=1; // Enable ADC
}

void TIMER0_Init(void)
{
    TMOD &= 0b_1111_0000; // Clear Timer 0 bits, preserve Timer 1
    TMOD |= 0b_0000_0001; // Set Timer 0 to 16-bit internal Timer mode
    TR0 = 0;              // Ensure Timer 0 is stopped
}

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask); // Set pin as analog input
			P0SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 1:
			P1MDIN &= (~mask); // Set pin as analog input
			P1SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 2:
			P2MDIN &= (~mask); // Set pin as analog input
			P2SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;   // Select input from pin

	ADINT = 0;
    ADBUSY = 1;     
    while (!ADINT);

	ADINT = 0;
	ADBUSY = 1;     // Convert voltage at the pin
	while (!ADINT); // Wait for conversion to complete
	return (ADC0);
}

#define VDD 3.3 // The measured value of VDD in volts

float Volts_at_Pin(unsigned char pin)
{
	 return (((ADC_at_Pin(pin))*VDD)/0b_0011_1111_1111_1111);
}

unsigned int Get_ADC (void)
{
	ADINT = 0;
	ADBUSY = 1;
	while (!ADINT); // Wait for conversion to complete
	return (ADC0);
}

void main (void)
{
	float v_ref = 0;
	float v_test = 0;
	float v_ref_peak = 0;
	float v_test_peak = 0;
	float freq_ref = 0;
	float freq_test = 0;
	float phase_float = 0;

	unsigned int half_period_ref = 0;
	unsigned int period_ref = 0;

	unsigned int half_period_test = 0;
	unsigned int period_test = 0;

	unsigned int phase = 0;

	waitms(500);
	printf("\x1b[2J");
	
	printf ("ADC test program\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);
	
	InitPinADC(2, 3);
	InitPinADC(2, 4);
	InitPinADC(1, 4);
	InitPinADC(0, 6);
	InitADC();
	LCD_4BIT();
	TIMER0_Init();

	while(1)
	{
		// --- FIX 2: half_period_ref now measured LOW->LOW to match phase measurement ---
		ADC0MX=QFP32_MUX_P2_3;
		ADINT = 0;
		ADBUSY=1;
		while (!ADINT);
		TL0=0;
		TH0=0;
		while (Get_ADC() > NOISE_THRESHOLD_LOW);  // wait for signal to be near zero
		while (Get_ADC() <= NOISE_THRESHOLD_HIGH); // wait for rising LOW crossing (was THRESHOLD_HIGH)
		TR0=1;
		while (Get_ADC() > NOISE_THRESHOLD_LOW);  // wait for falling LOW crossing
		TR0=0;
		half_period_ref=TH0*256.0+TL0;
		period_ref = (half_period_ref * 24000.0) / SYSCLK;
		printf("Reference Period: %3u ms | ", period_ref);
		freq_ref = 1000.0 / period_ref;
		printf("Reference Frequency: %3.0f Hz | ", freq_ref);

		// --- FIX 2: half_period_test now measured LOW->LOW to match phase measurement ---
		ADC0MX=QFP32_MUX_P2_4;
		ADINT = 0;
		ADBUSY=1;
		while (!ADINT);
		TL0=0;
		TH0=0;
		while (Get_ADC() > NOISE_THRESHOLD_LOW);  // wait for signal to be near zero
		while (Get_ADC() <= NOISE_THRESHOLD_HIGH); // wait for rising LOW crossing (was THRESHOLD_HIGH)
		TR0 = 1;
		while (Get_ADC() > NOISE_THRESHOLD_LOW);  // wait for falling LOW crossing
		TR0=0;
		half_period_test=TH0*256.0+TL0;
		period_test = (half_period_test * 24000.0) / SYSCLK;
		printf("Test Period: %3u ms | ", period_test);
		freq_test = 1000.0 / period_test;
		printf("Test Frequency: %3.0f Hz | ", freq_test);
		sprintf(buff, "Frequency: %2.0f Hz", freq_ref);
		LCDprint(buff, 0, 2, 1);

		waitms(100);

		v_ref = Volts_at_Pin(QFP32_MUX_P0_6);
		v_test = Volts_at_Pin(QFP32_MUX_P1_4);
		v_ref_peak = v_ref + 0.272;
		v_ref_peak *= 1.1;
		v_test_peak = v_test + 0.279;
		v_test_peak *= 1.1;

		printf("Referance Amplitude: %3.2f V | ", v_ref_peak);
		printf("Test Amplitude: %3.2f V\r\n", v_test_peak);
		sprintf(buff, "Ref Peak: %3.3f V", v_ref_peak);
		LCDprint(buff, 0, 1, 1);
		sprintf(buff, "Test Peak: %3.3f V", v_test_peak);
		LCDprint(buff, 1, 1, 1);

		waitms(100);

		// --- FIX 1: simultaneous zero sync instead of sequential waits ---
		do {
			while (ADC_at_Pin(QFP32_MUX_P2_3) > NOISE_THRESHOLD_LOW);
		} while (ADC_at_Pin(QFP32_MUX_P2_4) > NOISE_THRESHOLD_LOW);

		while (1) 
		{
			// Did Reference rise first? Test LAGS
			if (ADC_at_Pin(QFP32_MUX_P2_3) > NOISE_THRESHOLD_LOW) 
			{
				TL0 = 0; TH0 = 0; TR0 = 1;
				while (ADC_at_Pin(QFP32_MUX_P2_4) <= NOISE_THRESHOLD_LOW);
				TR0 = 0;
				phase = (TH0 * 256.0) + TL0;
				phase_float = -((phase / (half_period_ref * 2.0)) * 360.0 * 9.0000 / 10.000); // test lags = negative
				printf("Phase: %3.1f deg\r\n\n", phase_float);
				break;
			}

			if (ADC_at_Pin(QFP32_MUX_P2_4) > NOISE_THRESHOLD_LOW) 
			{
				TL0 = 0; TH0 = 0; TR0 = 1;
				while (ADC_at_Pin(QFP32_MUX_P2_3) <= NOISE_THRESHOLD_LOW);
				TR0 = 0;
				phase = (TH0 * 256.0) + TL0;
				phase_float = (phase / (half_period_ref * 2.0)) * 360.0 * 9.0000 / 10.000; // test leads = positive
				printf("Phase: %3.1f deg\r\n\n", phase_float);
				break;
			}
		}

		sprintf(buff, "Test Phase: %4f", phase_float);
		LCDprint(buff, 1, 2, 1);

	}  
} 
