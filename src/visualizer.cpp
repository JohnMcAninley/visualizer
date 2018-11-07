/*
* Each below component should be modular and easily replaced.
*
* A)	Free run ADC until the buffer is full.
*			NOTE: This will restart and continue while below happens.
*
*			i. 	Set up ADC to run as desired. This will consist of setting free
*			run mode and setting scale factor.
*
*			ii.	Set up circular buffer of desired length.
*
*			iii.Attach interrupt.
*
* B) 	Trigger interrupt when buffer is full.
*			NOTE: Once interrupt is attached, this should occur automatically.
*
* C) 	Possibly apply some preprocessing to the FFT input.
*
*			i.	Remove noise.
*
*			ii.	Apply dynamic range compression.
*
* D) 	Compute FFT of the sampled data.
*
*			i.	Custom FFT implementation in assembly. Will need helpers to
*				prepare input and extract output between C++ and assembly.
* 
* E) 	Possibly apply some post-processing to the FFT output.
*
*			i. Equalization (weighting FFT output bins)
*
* F) 	Interpret the data for visualization.
*
*			i.	Map N frequency bins to C color bins (consisting of RGB).
*			ii. Map to lightness.
*			iii.Fading.
*			iv.	Addressable: movement.
*
* G) 	Output the visualization.
*/

#include "include/CircularBuffer.h"

#define ADC_PRESCALER 16

/* 	REFERENCES
*	www.openmusiclabs.com/learning/digital/atmega-adc/
*	www.openmusiclabs.com/learning/digital/atmega-adc/in-depth/
*
*   | ADC Pre | ADC Clk | # of |          |          |
*   | -scalar | F (kHz) | Bits | Fs (kHz) | Fn (kHz) |
*   |---------|---------|------|----------|----------|
*   |   128   |    125  |  9.6 |    9.62  |    4.81  |
*   |    64   |    250  |  9.5 |   19.23  |    9.62  |
*   |    32   |    500  |  9.4 |   38.46  |   19.23  |
*   |    16   |   1000  |  8.7 |   76.92  |   38.46  |
*   |     8   |   2000  |  7.4 |  153.85  |   76.92  |
*   |     4   |   4000  |  5.9 |  307.69  |  153.85  |
*   |     2   |    N/A  |  N/A |  615.38  |  307.69  |
*/
#if ADC_PRESCALAR == 128
	#define ADC_BITS 9
#elif ADC_PRESCALAR == 64
	#define ADC_BITS 9
#elif ADC_PRESCALAR == 32
	#define ADC_BITS 9
#elif ADC_PRESCALAR == 16
	#define ADC_BITS 8
#elif ADC_PRESCALAR == 8
	#define ADC_BITS 7
#elif ADC_PRESCALAR == 4
	#define ADC_BITS 5
#else
	#error Prescalar must be one of {4, 8, 16, 32, 64, 128}.
#endif

// Optionally override ADC_BITS to be <= 8 to use a single byte even if this
// means discarding some information from the ADC.
// #define ADC_BITS 8

#if ADC_BITS > 8
typedef uint16_t adc_data_t;
#elif
typedef uint8_t adc_data_t;
#endif

#define ADC_PIN 0

volatile bool buf_full;
volatile CircularBuffer ping(BUFF_SIZE);
volatile CircularBuffer pong(BUFF_SIZE);
volatile CircularBuffer *capt_buf, *proc_buf;

/*
*	Register a handler for the ADC conversion complete interrupt (ADC_vect).
*
#	NOTE: 	The ISR macro generates some additional code, which may not be
*			desirable. If so, pass additional argument ISR_NAKED to ISR and be
*			sure to add reti() to the handler epilogue. Compiler generated 
*			pro/epi-logues can be up to 82 cycles of possibly unnecessary
*			instructions, so this may be worth it.
*
*	NOTE:	Depending on overhead and work performed in ISR, it may be worth
*			it to simply disable interrupts and poll the desired condition in a 
*			tight loop in main code.
*
*	REFERENCE:	http://www.gammon.com.au/interrupts
*/
ISR(ADC_vect) {
	// Read in data from the ADC register.
	//
	// Reading ADCL locks both ADCL and ADCH so they must be read 
	// in this order and must both be read even if output is only 8 bits.
	//
	// Optionally, some noise thresholding could be applied here, but for
	// modularity the data is simply collected as is and preprocessing is
	// applied later.
	#if ADC_BITS > 8
	capt_buf->write(ADC);
	#else
	capt_buf->write(ADCH);
	#endif

	// TODO	Relative occurence of full buffer and already processing 
	// 		(this order) and not processing and not full (switch order). 
	if (!processing && capt_buf->full()) {
		// Set a flag so the other buffer continues to fill/overwrite and 
		// doesn't start to process until this buffer is done.
		// This flag also informs the main loop to start processing this buffer.
		processing = true;

		// TODO	Will the other buffer continuing to fill (meaning this ISR being
		// 		called after every ADC conversion) interrupt the processing?

		// Swap the collection and processing buffers.
		CircularBuffer *tmp = proc_buf;
		proc_buf = capt_buf;
		capt_buf = tmp;
	}
}

/*
*	Initialize the onboard ADC.
*
*	REFERENCES:	www.gammon.com.au/adc
*				www.instructables.com/id/Girino-Fast-Arduino-Oscilloscope/
*/
void init_adc() {
	
	sbi(ADCSRA, ADEN);			// Enable the ADC

	// Start the first conversion in Free Run mode. 
	// NOTE: first converstion takes 25 cycles instead of 13.
	sbi(ADCSRA, ADSC);

	// TODO	The two above statements should most likely be moved to the end of 
	// 		all performed initializations.

	// Attach interrupt to handle completed conversion.
	// Enable the analog comparator interrupt.
	// sbi(ACSR, ACIE);
	// Enable the ADC conversion complete interrupt.
	sbi(ADCSRA, ADIE);

	// Set the ADC mode to Free Run.
	// The mode is controlled by ADTS[2:0] in ADCSRB with Free Run mode defined
	// as ADTS == 0b000. 	
	cbi(ADCSRB, ADTS2);
	cbi(ADCSRB, ADTS1);
	cbi(ADCSRB, ADTS0);		

	// Set prescalar to 32, ADC clock 500kHz, effective sampling rate ~38.5kHz, 
	// effective resolution 8 bits.
	// In the future this may be changed to 16, lowest prescalar for accurate 
	// results, ADC clock 1MHz, effective sampling rate ~76.9kHz, 
	// effective resolution 8 bits.
	// Relationship between prescalar values, Arduino ADC frequencies, 
	// and effective resolution can be found in the above reference.
	// sbi(ADCSRA, ADPS[2:0]);
	// The prescalar is equal to the value of these 3 bits left shifted once.
	// TODO	This is hardcoded as 32, set this based on the ADC_PRESCALAR macro.
	sbi(ADCSRA, ADPS2);
	cbi(ADCSRA, ADPS1);
	sbi(ADCSRA, ADPS0);
}

/*
*	Initialize the analog pins and circuitry.
*
*	REFERENCES:	www.instructables.com/id/Girino-Fast-Arduino-Oscilloscope/
*/
void init_analog() {
	// Disable the digital buffers for the analog input pins as all will either
	// be used as analog input pins or not at all. This is done to eliminate the
	// noise generated by the digital buffers switching when an analog signal is
	// applied to them.
	sbi(DIDR0, ADC5D);
	sbi(DIDR0, ADC4D);
	sbi(DIDR0, ADC3D);
	sbi(DIDR0, ADC2D);
	sbi(DIDR0, ADC1D);
	sbi(DIDR0, ADC0D);

	// Use external reference voltage of v Volts connected to AREF pin. This 
	// should be connected to GND through a ~1uF smoothing resistor.
	// TODO	This should be replaced by setting ADMUX register directly so that 
	// 		the reference voltage is immediately connected.
	analogReference(EXTERNAL);
	// cbi(ADMUX, REFS1);
	// sbi(ADMUX, REFS1);

	// If <= 8 bits of resolution are needed from the ADC, the results are left
	// shifted in the output register so the entire result is in ADCH. This is 
	// done because reading ADCL locks both registers until ADCH is read.
	// Setting ADLAR left shifts the results, clearing it right shifts them.
	#if ADC_BITS <= 8
	sbi(ADMUX, ADLAR);
	#else
	cbi(ADMUX, ADLAR);
	#endif

	// TODO	Set ADMUX to correct input pin as we should only be using a single 
	//		analog pin.
	ADMUX |= (ADC_PIN & 7);
}

void setup() {

	init_analog();
	init_adc();

	capt_buf = &ping;
	proc_buf = &pong;

	processing = false;

	sei();		// Enable interrupts
}

void loop() {
	// Wait for the buffer to fill.
	while(!processing);

	preprocess();

	fft_input();
	fft_execute();
	fft_output();

	postprocess();

	
}