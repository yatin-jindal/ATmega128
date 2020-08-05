#include <avr/io.h>
#include <avr/interrupt.h>

uint8_t hmData[18];

volatile uint8_t count;
volatile uint8_t add = 0, addr = 0;

int times=0;
int repeat=0;
uint8_t temp = 0x00;

void setupTimer(void){
	//enabling output compare interrupt
	TIMSK = (1<<OCIE1A);
	//putting the value to compare counter with
	OCR1A = 0x3D08;
	//selecting clock, enabling CTC mode (Clear Timer on Compare) 
	TCCR1B = (1<<CS12)|(1<<CS10)|(1<<WGM12);
	count=0;
}

void initInterrupt(void){
	//setting to generate interrupt request at rising edge
	EICRA = (1<<ISC20)|(1<<ISC21)|(1<<ISC30)|(1<<ISC31);
	//setting to generate interrupt request when logic changes
	EICRB = (1<<ISC40)|(1<<ISC50)|(1<<ISC60)|(1<<ISC70);
	//enabling external interrupts
	EIMSK = (1<<INT2)|(1<<INT3)|(1<<INT4)|(1<<INT5)|(1<<INT6)|(1<<INT7);
}

void adcInit(void){
	//enabling ADC, setting frequency prescaler to 16
	ADCSRA |= (1<<ADEN)|(1<<ADPS2);
}

void i2cInit(void){
	//setting division factor for SCL to 32
	TWBR=0x20;
	//setting prescaler to 1
	TWSR=0x00;
	add=0x00;
}

void i2cStart(void){
	//clearing interrupt flag, generate start condition, enable twi operation
	TWCR=0b10100100;
	//waiting for interrupt flag to set
	while(!(TWCR & (1<<TWINT)));
	//checking status register
	while((TWSR & 0xF8)!= 0x08);
}

void eepromWr(void){
	//loading SLA+W 
	TWDR=0b10100000;
	//clear TWINT
	TWCR=(1<<TWINT)|(1<<TWEN);
	//wait for flag to set
	while (!(TWCR & (1<<TWINT)));
	//check status register
	while( (TWSR & 0xF8) != 0x18);
}

void address(uint8_t addrs){
	/*sends to eeprom the address to write at
	*Parameters
	*addrs     address at which to write
	*/
	TWDR=addrs;
	//clear TWINT
	TWCR=(1<<TWINT)|(1<<TWEN);
	//wait for flag to set
	while (!(TWCR & (1<<TWINT)));
	//check status register
	while( (TWSR & 0xF8) != 0x28);
}

void eepromWrite(unsigned char data){
	//load data into TWDR
	TWDR=data;
	//clear TWINT
	TWCR=(1<<TWINT)|(1<<TWEN);
	//wait for flag to set
	while (!(TWCR & (1<<TWINT)));
	//check status register
	while( (TWSR & 0xF8) != 0x28);
	//increment add for next write operation
	add+=0x01;
}

unsigned char eepromRandomRead(void){
	/*returns the data stored in EEPROM at the prev sent address
	*/
	//sending repeated start condition
	TWCR=0b10100100;      
	//waiting for flag to set
	while(!(TWCR & (1<<TWINT)));
	//checking status register
	while((TWSR & 0xF8)!= 0x10);
	
	//loading SLA+R
	TWDR=0b10100001; 
	//clear TWINT
	TWCR=(1<<TWINT)|(1<<TWEN);
	//waiting for flag to set
	while (!(TWCR & (1<<TWINT)));
	//check status register
	while( (TWSR & 0xF8) != 0x40);
	
	// TWEA not set as we have to send nack
	TWCR = (1<<TWINT)|(1<<TWEN);  
	//waiting for flag to set
	while (!(TWCR & (1<<TWINT)));
	//check status register
	while(((TWSR & 0xF8)!=0x58));
	
	return(TWDR);
}


void i2cStop(void){
	//sending stop conditon
	TWCR=0b10010100;
}

void adcCheck(uint8_t mux, int i, uint8_t idealValue){
	/*Writes the adc value into EEPROM
	*parameters
	*mux     analog channel
	*/
	//setting external voltage reference, left adjust result
	ADMUX = 0x20;
	ADMUX |= mux;
	//start conversion
	ADCSRA |= (1<<ADSC);
	//wait for conversion to finish
	while(!(ADCSRA & (1<<ADIF)));
	
  hmData[i]=ADCH;
  
  if(abs(ADCH-idealValue) > 0.5) opMode=Emergency;
}

void uartInit (void){
	//setting baud rate to 9600
	UBRR0H=0x00;
	UBRR0L=0x33;
	//setting no. of data bits to 8
	UCSR0C=0x06;
	//enable receive complete interrupt, receiver enable, transmitter enable
	UCSR0B=0x98;
}


void spiInit(void){
	//set MISO as output
	DDRB = (1<<3);
	//enable SPI and SPI interrupt
	SPCR |=(1<<SPIE)|(1<<SPE);
}

uint8_t spiTrans(uint8_t data){
	//load data into SPDR
	SPDR = data;
	// Wait for reception complete 
	while(!(SPSR & (1<<SPIF)));
	//Return data register
	return  SPDR;
}


void HMDataCheck(freq){
	//setting the pin status variables to their starting value
	hmData[5]=0x00;
	hmData[9]=0x00;
	hmData[6]=0x00;
	//setting up the timer with the required settings and initializing the various communication protocols used
	setupTimer();
	adcInit();
	uartInit();
	spiInit();
	i2cInit();
	//enabling the external interrupts and enabling global interrupts
	initInterrupt();
	sei();
	
	while (opmode!=Emergency)
	{
		//this is the code that gets executed every minute, and writes the HM data in the EEPROM
		if (count==(freq/2))
		{
			//resetting the count variable and starting the i2c connection and sending the data to the EEPROM
			count=0;
			i2cStart();
			eepromWr();
			address(0x00);
			address(add);
			for (int i=0; i<18 ;i++){
				eepromWrite(hmData[i]);
			}
			i2cStop();
			}
      
    else 
    {
    	adcCheck(0,7,42);
      adcCheck(1,8,42);
      adcCheck(2,10,42);
      adcCheck(3,11,42);
      adcCheck(4,12,42);
      adcCheck(5,13,42);
      adcCheck(6,14,42);
      adcCheck(7,15,42);
    }
		
	}
}

ISR(USART0_RX_vect)
{
	/*this is the interrupt for the USART communicstion, for getting the HM data from comm microcontroller*/
	if (repeat==0)
	{
		//reading the data from the USART data register
		hmData[16]=UDR0;
		repeat++;
	}
	else
	{
		hmData[17]=UDR0;
		repeat=0;
	}
}

ISR(SPI_STC_vect)
{	
	/*this is the interrrupt for SPI communication from the PS4 OP microcontroller, switch statement is used to store the data in different variables*/
	switch(times)
	{
		//cases 0-4 is getting operational mode and timestamp from PS4 OP microcontroller
		case 0 :
		hmData[4]=SPDR;
		times++;
		break;
			
		case 1 :
		hmData[0]=SPDR;
		times++;
		break;
			
		case 2 :
		hmData[1]=SPDR;
		times++;
		break;
			
		case 3 :
		hmData[2]=SPDR;
		times++;
		break;
			
		case 4 :
		hmData[3]=SPDR;
		times++;
		break;
		//this case is for reading the HM data from the EEPROM and sending it to PS4 for telemetry	
		case 5 :
		i2cStart();
		eepromWr();
		address(0x00);
		address(addr);
		temp = SPDR;
		SPDR = eepromRandomRead();
		i2cStop();
		//ensuring the data gets read to a certain address 
		if(addr == 0x09) times = 0;
		else times = 5;
		addr++;
		break;
		
	}
}
/*These are the external interrupts for various pin statuses that we are including in the HM data, that we are storing in variables*/
ISR(TIMER1_COMPA_vect)
{
	count++;
}

ISR(INT2_vect)
{
	hmData[5] |= 0x30;
}

ISR(INT3_vect)
{
	hmData[5] |= 0x03;
}

ISR(INT4_vect)
{
	hmData[6] |= 0x30;
}

ISR(INT5_vect)
{
	hmData[6] |= 0x03;
}

ISR(INT6_vect)
{
	hmData[9] |= 0x30;
}

ISR(INT7_vect)
{
	hmData[9] |= 0x03;
}
