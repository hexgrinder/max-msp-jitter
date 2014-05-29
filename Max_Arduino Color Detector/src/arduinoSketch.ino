/*
arduinoSketch.ino

Functional Overview:

>> Sends a message on the transmit buffer indicating that the 
ON / OFF switch has been toggled.  This is implemented using
an interrupt request handler on pin INT0.

>> Receives character messages from the Serial buffer.  Loads 
the corresponding LED sequence on PORT B based on the character 
received.

@author Michael Layman
*/

#include <avr/interrupt.h>   // Macro ISR()
#include <avr/io.h>

char _receivedByte = 0;

void setup() {
    
    // Set LED pins to OUTPUT direction. Use Port B.
    asm volatile(
        "ldi r18, 0x0f \n\t"  // r18 = 0fh = (0000 1111)2. Pins 0 through 3
        "out %0, r18   \n\t"    // Sets PB[0-3] as output
        :: "I" (_SFR_IO_ADDR(DDRB))
    );

    /* Steps to setup interrupt
       1) Set interrupt pin as INPUT
       2) Enable the interrupt pin
       3) Enable the interrupt pin's mask
       4) Enable global interrupt 
    */
    
    // Set Interrupt Request pin to receive. Use INT0. INT0 is on Port D.
    
    // Sets INT0 as input
    asm volatile(
        "cbi %0, %1 \n\t"
        :: "I" (_SFR_IO_ADDR(DDRD)), "I" (DDD2)
    );
    
    // enable INT0
    EICRA |= 0x01;     // EICRA OR 1:  Any logical change on INT0 generates an interrupt request.
    // enable mask for INT0
    EIMSK |= 0x01;     // EIMSK OR 1
    // enable global interrupt
    asm volatile (
        "sei  \n\t"          
    );
    
    // Startup test
    ledTest();
    
    // Start communication with MAX
    Serial.begin(9600);  
}

void loop() {
    // If message from MAX...
    if (Serial.available() > 0) {
        // Check message from MAX
        _receivedByte = Serial.read();
        // Light up corresponding pin based on message
        lightLED(_receivedByte);
    }
}

/*
    INT0 Interrupt Handler.
    
    Sends outs the character 'C' on the serial connection.
    
    (Source: ATMEL Manual - p179 for code to send data)
    
    // *** Assembly Code
    
    USART_Transmit:
    ; Wait for empty transmit buffer
    in r16, UCSRnA
    sbrs r16, UDREn
    rjmp USART_Transmit
    ; Put data (r16) into buffer.  load 'C' to indicate camera toggle
    ldi r16, 'C'
    ; sends the data
    out UDRn,r16
    ret
    
    // *** C Code
    
    // Wait for empty transmit buffer 
    while ( !( UCSR0A & (1<<UDRE0)) )
    ;
    // Put data into buffer, sends the data 
    UDR0 = 0x43;

*/
ISR(INT0_vect) {
    
    // HACK: Map local variable to r17 register,
    // so I can send it later.
    register char val asm("r17");
    
    asm volatile(
        "cli \n\t"
        "push %0 \n\t"
            "USART_Transmit: \n\t"
                "st X, %1 \n\t" // Get status from UCSR0A
                "ld r16, X \n\t"
                "sbrs r16, %2 \n\t" // Check if UDRE0 is set (ie: UDR0 register is ready to transmit)
                "rjmp USART_Transmit \n\t"  // Wait for empty transmit buffer 
            "ldi r17, 0x43 \n\t"  // val = 'C' (load 'C' to indicate camera toggle)
        "pop %0 \n\t"
        "sei \n\t"
        :
        : "r" (SREG), "r" (_SFR_IO_ADDR(UCSR0A)), "I" (UDRE0)
    );
    
    // Had to do it this way b/c HW limits access of IN/OUT instructions to 
    // (0x3f) and below; UDRO resides in (0xc6).  Could not use LD/ST b/c
    // those instructions require constants.
    UDR0 = val; // puts data on the line and sends it
}

// *** HELPERS ***

/* 
Translates color encodings into their corresponding LED lighting display.

Encodings (Code >> Response):
    * 'r' >> Red led lit, all others off
    * 'g' >> Green led lit, all others off
    * 'b' >> Blue led lit, all others off
    * OTHER_CHARS >> NO_COLOR led lit, all others off

@param code - Color key
*/ 
void lightLED(char code) {
    switch (code) {
        case 'r':
            setLEDSequence(1);
            break;
        case 'g':
            setLEDSequence(2);
            break;
        case 'b':
            setLEDSequence(3);
            break;
        default:
            setLEDSequence(0);
            break;
    }
}

/*
Translates integer into LED display sequence.

Encodings (ledPin >> Response):
    * 1 >> Red led lit, all others off
    * 2 >> Green led lit, all others off
    * 3 >> Blue led lit, all others off
    * 0 or other numbers >> NO_COLOR led lit, all others off
    
Notes:
>> LED sequence will be loaded to PORTB.
>> Assumes PORTB will sink current as LEDs are lit 
(i.e.: Set bit high to turn off, set low to turn on).

@param ledPin - Assigned LED pin to turn on
*/ 
void setLEDSequence(int ledPin) {
    
    switch (ledPin) {
        case 1:
            asm volatile ("ldi r16, 0x0d \n\t"); // Red LED: 0000 1101
            break;
        case 2:
            asm volatile ("ldi r16, 0x0b \n\t"); // Green LED: 0000 1011
            break;
        case 3:
            asm volatile ("ldi r16, 0x07 \n\t"); // Blue LED: 0000 0111
            break;
        case 0:
        default:
            asm volatile ("ldi r16, 0x0e \n\t"); // NoColor LED: 0000 1110
            break;
    }

    // Load the LED sequence to PORTB.
    asm volatile (
      "out %0, r16 \n\t" 
      :
      : "I" (_SFR_IO_ADDR(PORTB))
    );
}

/*
Led test.  Cycles on each led for 1 second.
*/
void ledTest(void) {
    delay(1000);              
    setLEDSequence(1);
    delay(1000);              
    setLEDSequence(2);
    delay(1000);              
    setLEDSequence(3);
    delay(1000);              
    setLEDSequence(0);
}
