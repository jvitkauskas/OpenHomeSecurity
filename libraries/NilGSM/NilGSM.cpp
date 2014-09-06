/**
 *       
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "NilGSM.h"

// Declare and initialize the semaphore.
static SEMAPHORE_DECL(GSMNewMsg, 0);

// both buffers are static global vars in this file. interrupt handlers
// need to access them
static gsm_rx_ring_buffer rx_buffer;
static gsm_tx_ring_buffer tx_buffer;
// Local static variables
static uint8_t _msg;

/**
 * Initialize the USART module with the BAUD rate predefined
 * in HardwareSerial.h
 *
 * This may implement passing addresses of registers, like the HardwareSerial
 * class from the Arduino library, but that's not necessary at this point.
 *
 */
NilGSM::NilGSM(gsm_rx_ring_buffer *rx_buffer_ptr, gsm_tx_ring_buffer *tx_buffer_ptr) {
 //   _rx_buffer = rx_buffer_ptr;
 //   _tx_buffer = tx_buffer_ptr;
}

/**
 * enable receiver, transmitter ...
 *    
 */
void NilGSM::begin(unsigned long baud) {
  uint16_t baud_setting;
  // don't worry, the compiler will squeeze out F_CPU != 16000000UL
  if (F_CPU != 16000000UL || baud != 57600) {
    // Double the USART Transmission Speed
    UCSR0A = 1 << U2X0;
    baud_setting = (F_CPU / 4 / baud - 1) / 2;
  } else {
    // hardcoded exception for compatibility with the bootloader shipped
    // with the Duemilanove and previous boards and the firmware on the 8U2
    // on the Uno and Mega 2560.
    UCSR0A = 0;
    baud_setting = (F_CPU / 8 / baud - 1) / 2;
  }
  // assign the baud_setting
  UBRR0H = baud_setting >> 8;
  UBRR0L = baud_setting;
  // enable transmit and receive
  UCSR0B |= (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);

  _msg = 0;

}

/**
 * Fulush RX buffer
 */
void NilGSM::flushRX(void) {
  rx_buffer.tail = rx_buffer.head;
  _msg = 0;
} 

/**
 * Read procedure
 */
uint8_t NilGSM::read(void) {
  uint8_t datal  = rx_buffer.buffer[rx_buffer.tail];
  rx_buffer.tail = (rx_buffer.tail + 1) % GSM_USART_RX_BUFFER_SIZE;
  if (datal == 0x0A) { // NL
    _msg --;
  }
  return datal;
} 

/**
 * Read message
 */
uint8_t NilGSM::read(uint8_t *msg) {
  if (_msg == 0) return 0; // no message
  uint8_t count = 0;
  uint8_t rb;
  do {
    rb = read();
    if (rb != 0x0A & rb != 0x0D) {
      msg[count] = rb;  // not CR and NL
      count++;          // We have at least one byte present
    }
  } while (rb != 0x0A); // NL
  return count;
} 

/**
 * Messages count
 */
uint8_t NilGSM::isMsg(void) {
  return _msg;
} 


/**
 * Receive handler
 */
NIL_IRQ_HANDLER(USART0_RX_vect) {
  
  /* On AVR this forces compiler to save registers r18-r31.*/
  NIL_IRQ_PROLOGUE();
  /* Nop on AVR.*/
  nilSysLockFromISR();
  

  uint8_t datal  = UDR0;
  rx_buffer.buffer[rx_buffer.head] = datal;  // save byte
  rx_buffer.head = (rx_buffer.head + 1) % GSM_USART_RX_BUFFER_SIZE;

  if (datal == 0x0A) { // NL
    _msg ++;
  }


  if ((rx_buffer.head) == rx_buffer.tail) { // full, we are loosing data
    rx_buffer.tail = (rx_buffer.tail + 1) % GSM_USART_RX_BUFFER_SIZE;
  }
  
  /* Nop on AVR.*/
  nilSysUnlockFromISR();
  /* Epilogue performs rescheduling if required.*/
  NIL_IRQ_EPILOGUE();
}


/**
 * Data Register Empty Handler
 */
NIL_IRQ_HANDLER(USART0_UDRE_vect) {
  /* On AVR this forces compiler to save registers r18-r31.*/
  NIL_IRQ_PROLOGUE();
  /* Nop on AVR.*/
  nilSysLockFromISR();

  if (tx_buffer.head == tx_buffer.tail) {
      UCSR0B &= ~(1<<UDRIE0); // Buffer is empty, disable the interrupt
      //tx_buffer.head = tx_buffer.tail = 0;
  } else {
      UDR0 = tx_buffer.buffer[tx_buffer.tail];
      tx_buffer.tail = (tx_buffer.tail+1) % GSM_USART_TX_BUFFER_SIZE;
  }
  
  /* Nop on AVR.*/
  nilSysUnlockFromISR();
  /* Epilogue performs rescheduling if required.*/
  NIL_IRQ_EPILOGUE();
}

/**
 * Transmit Complete Interrupt Handler
 *  
 * Automatically cleared when the interrupt is executed. 
 
NIL_IRQ_HANDLER(USART0_TX_vect) {
  
  NIL_IRQ_PROLOGUE();
  nilSysLockFromISR();  
  
  nilSysUnlockFromISR();
  NIL_IRQ_EPILOGUE();  
}
*/

size_t NilGSM::write(uint8_t data) {
  
  // Save data byte at end of buffer
  tx_buffer.buffer[tx_buffer.head] = data;
  // Increment the head
  tx_buffer.head = (tx_buffer.head+1) % GSM_USART_TX_BUFFER_SIZE;

  if ((tx_buffer.head) == tx_buffer.tail) { // full, we are loosing data
    tx_buffer.tail = (tx_buffer.tail + 1) % GSM_USART_TX_BUFFER_SIZE;
  }

  UCSR0B |= (1<<UDRIE0);  // Enable Data Register Empty interrupt, start transmitting
  //UCSR0B |= (1<<TXCIE0);  // Enable Transmit Complete interrupt (USART_TXC) 
  return 1;
}

void NilGSM::printP(const unsigned char *str) {
  while (uint8_t value = pgm_read_byte(str++)) {
    write(value);
  }
}

//
uint8_t NilGSM::ATWaitMsg(void){ 
  uint8_t at_wait = 0;

  while ((!isMsg()) & (at_wait < AT_WAIT)) {
    nilThdSleepMilliseconds(AT_DELAY);
    at_wait++;
  }
  if (at_wait == AT_WAIT) return 0;
  else return 1;
} 

// 
int8_t NilGSM::ATsendCmd(char* what){ 
  uint8_t t_size;
  uint8_t text[10];
  int8_t  at_tmp;

  //[SEND] AT
  //AT
  //OK
  flushRX();

  println(what);

  if (!ATWaitMsg()) return 0;              // timeout reached
  t_size = read(text);                     // read serial
  //  WS.print(t_size);
  //  for(uint8_t i = 0; i < t_size; i++) {
  //   WS.print((char)text[i]);
  //  }
  //  WS.println();
  if (t_size < 2) return -11;              // echo not match, ?? sizeof(what) returns always 2 for any lenght
  at_tmp = memcmp(what, text, t_size);
  //println(at_tmp);
  if (at_tmp != 0) return -1;              // echo not match

  if (!ATWaitMsg()) return 0;              // timeout reached
  t_size = read(text);                     // read serial
  if (t_size != 2) return -22;             // 'OK' size
  at_tmp = memcmp(AT_OK, text, t_size);    // compare
  if (at_tmp != 0) return -2;              // OK not received
  else return 1;
}

// 
int8_t NilGSM::ATsendCmdWR(char* what, uint8_t* response){ 
  uint8_t t_size, r;
  uint8_t text[10];
  int8_t  at_tmp;

  //[SEND] AT+CSQ
  //AT+CSQ
  //+CSQ: 24,0
  //
  //OK
  flushRX();

  println(what);
  // get echo
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size < 3) return -11;               // echo not match
  at_tmp = memcmp(what, text, t_size);
  if (at_tmp != 0) return -1;               // echo not match

  // get output
  if (!ATWaitMsg()) return 0;               // timeout reached
  r = read(response);                       // read serial
  response[r] = 0;                          // terminate the response by null

  // empty line
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size != 0) return -33;              // not empty line
  
  // get OK / ERROR
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size != 2) return -22;              // 'OK' size
  at_tmp = memcmp(AT_OK, text, t_size);     // compare
  if (at_tmp != 0) return -2;               // OK not received
  else return r;                            // size of reply
}

// 
int8_t NilGSM::ATsendCmdWR(char* what, uint8_t* response, uint8_t index){ 
  uint8_t t_size, r;
  uint8_t text[10];
  int8_t  at_tmp;
  char * pch;

  //[SEND] AT+CSQ
  //AT+CSQ
  //+CSQ: 24,0
  //
  //OK
  flushRX();

  println(what);
  // get echo
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size < 3) return -11;               // echo not match
  at_tmp = memcmp(what, text, t_size);
  if (at_tmp != 0) return -1;               // echo not match

  // get output
  if (!ATWaitMsg()) return 0;               // timeout reached
  r = read(response);                       // read serial
  response[r] = 0;                          // terminate the response by null

  // get index
  pch = strtok ((char*)response," ,.-");
  r = 0;
  while (pch != NULL){
    ++r;
    if (r == index) break;
    pch = strtok (NULL, " ,.-");
  }
  strncpy ((char*)response, pch, sizeof(pch) );
  response[sizeof(pch)] = 0; 

  // empty line
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size != 0) return -33;              // not empty line
  
  // get OK / ERROR
  if (!ATWaitMsg()) return 0;               // timeout reached
  t_size = read(text);                      // read serial
  if (t_size != 2) return -22;              // 'OK' size
  at_tmp = memcmp(AT_OK, text, t_size);     // compare
  if (at_tmp != 0) return -2;               // OK not received
  else return r;                            // size of reply
}


/** Sleep while waiting for new message.
 * @note This function should not be used in the idle thread.
 * @return true if success or false if overrun or error.
 */
bool NilGSM::nilWaitGSMNewMsg() {
  // Idle thread can't sleep.
  if (nilIsIdleThread()) return false;
  if (nilSemWaitTimeout(&GSMNewMsg, TIME_IMMEDIATE) != NIL_MSG_TMO) return false;
  nilSemWait(&GSMNewMsg);
  return true;
}

NilGSM GSM(&rx_buffer, &tx_buffer);