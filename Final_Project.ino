//  Name: Ainsley Nutting
//  Assignment: Final Project
//  Class: CPE 301 Spring 2023
//  Date: 05/3/23

#include <RTClib.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <AccelStepper.h>

//define humidity and temperature sensor pins
#define DHT11_PIN 13
#define DHT_TYPE DHT11

//define water level sensor pins
#define water_lvl_power_pin 52
#define water_lvl_signal_pin A15

//define stepper motor step constant
#define FULLSTEP 4
#define STEP_PER_REVOLUTION 2048

//define button pins
volatile unsigned char* port_b = (unsigned char*) 0x25; 
volatile unsigned char* ddr_b  = (unsigned char*) 0x24; 
volatile unsigned char* pin_b  = (unsigned char*) 0x23; 

#define RDA 0x80
#define TBE 0x20  

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
 
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//define stepper motor pins
AccelStepper stepper(FULLSTEP, 31, 35, 33, 37);

const int rs = 4, en = 5, d4 =6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

RTC_DS1307 rtc;
DHT dht(DHT11_PIN, DHT_TYPE);

//define fan registers
volatile unsigned char* port_l_E = (unsigned char*) 0x10B; 
volatile unsigned char* ddr_l_E  = (unsigned char*) 0x10A; 
volatile unsigned char* pin_l_E  = (unsigned char*) 0x109; 

volatile unsigned char* port_l_A = (unsigned char*) 0x10B; 
volatile unsigned char* ddr_l_A  = (unsigned char*) 0x10A; 
volatile unsigned char* pin_l_A  = (unsigned char*) 0x109; 

volatile unsigned char* port_l_B = (unsigned char*) 0x10B; 
volatile unsigned char* ddr_l_B  = (unsigned char*) 0x10A; 
volatile unsigned char* pin_l_B  = (unsigned char*) 0x109; 

// Timer Pointers
volatile unsigned char *myTCCR1A  = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B  = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C  = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1  = (unsigned char *) 0x6F;
volatile unsigned char *myTIFR1   = (unsigned char *) 0x36;
volatile unsigned int  *myTCNT1   = (unsigned int *) 0x84;

//------------set up flags-----------
volatile bool water_lvl_flag = 0; 
volatile bool temp_flag = 0;
bool          start_flag = 0; 
volatile bool stop_flag = 0;
volatile bool reset_flag = 0;
volatile bool state_change_A = 0;
volatile bool state_change_B = 0;

volatile char state_status;

//global ticks counter
volatile unsigned int currentTicks = 65535;
bool timer_running = 0;


//define LED pins
//yellow LED register 40 pin 1
volatile unsigned char* port_g = (unsigned char*) 0x34; 
volatile unsigned char* ddr_g  = (unsigned char*) 0x33; 
volatile unsigned char* pin_g  = (unsigned char*) 0x32; 

//green LED pin 38
volatile unsigned char* port_d = (unsigned char*) 0x2B; 
volatile unsigned char* ddr_d  = (unsigned char*) 0x2A; 
volatile unsigned char* pin_d  = (unsigned char*) 0x29; 

//red and blue LED registers
volatile unsigned char* port_c = (unsigned char*) 0x28; 
volatile unsigned char* ddr_c  = (unsigned char*) 0x27; 
volatile unsigned char* pin_c  = (unsigned char*) 0x26; 


//variable for temp/humidity readings
unsigned int temp_threshold = 74;
unsigned int water_lvl_threshold = 100;
int i = 0;

//variables for potentiometer
int pot_Val = 0;
int pot_Previous = 0;
int long pot_Newval = 0;

void setup() {

  // setup the UART
  U0init(9600);
  // setup the ADC
  adc_init();

  setup_timer_regs();

  //begin real time clock
  rtc.begin();

  //start LCD display
  lcd.begin(16, 2);

  //start temp and humidity readings
  dht.begin();

  //set rtc module
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  //set up fan pins to output
  *ddr_l_E |= 0x02;
  *ddr_l_A |= 0x08;
  *ddr_l_B |= 0x20;

  //set stepper motor speed
  stepper.setMaxSpeed(4800);
  stepper.setAcceleration(4800);

  //initialize pushbuttons as inputs
  *ddr_b &= 0xEF;
  *ddr_b &= 0xDF;
  *ddr_b &= 0xBF;

  //enable pullup resistor on pushbuttons
  *port_b |= 0x10;
  *port_b |= 0x20;
  *port_b |= 0x40;

  //set state status to Disabled to start
  state_status = 'D';

  //set LED pins to output
  *ddr_g |= 0x02;
  *ddr_d |= 0x80;
  *ddr_d |= 0x08;
  *ddr_d |= 0x20;

  //set water level power pin to output
  *ddr_b |= 0x02;

  //turn water level sensor off
  *port_b &= 0xFD;

  // setup the UART
  U0init(9600);
  // setup the ADC
  adc_init();

}

void loop() {
  pot_Val = adc_read(A0);
  if((pot_Val > pot_Previous+6) || (pot_Val < pot_Previous-6)){
    pot_Newval = map(pot_Val, 0, 1023, 0, 1600);
    stepper.runToNewPosition(pot_Newval);
    pot_Previous = pot_Val;
  }
  if(state_status == 'D'){
    enter_Disabled();
    if (start_flag == 1){
      state_status = 'I';
      exit_Disabled();
    }
  }
  else if(state_status == 'I'){
    enter_Idle();
    if (water_lvl_flag == 1){
      exit_Idle();
      state_status = 'E';
    }
    else if(temp_flag == 1){
      exit_Idle();
      state_status = 'R';
    }
    else if(stop_flag == 1){
      exit_Idle();
      state_status = 'D';
    }
  }
  else if(state_status == 'E'){
    enter_Error();
      if(reset_flag == 1){
        exit_Error();
        state_status = 'I';
      }
      else if(stop_flag == 1){
        exit_Error();
        state_status = 'D';
      }
      
  }
  else if(state_status == 'R'){
    enter_Running();
      if(temp_flag == 1){
        exit_Running();
        state_status = 'I';
      }
      else if(water_lvl_flag == 1){
        exit_Running();
        state_status = 'E';
      }  
      else if(stop_flag == 1){
        exit_Running();
        state_status = 'D';
      }
  }
  else{
    Serial.write("WTF 0_0'");
  }
  
}

void enter_Disabled(){
  //-----print state transition to serial port with time from real time clock
  state_change_A = 1;
  if(state_change_A != state_change_B){
    U0putchar('T'); 
    U0putchar('r');
    U0putchar('a'); 
    U0putchar('n'); 
    U0putchar('s'); 
    U0putchar('i'); 
    U0putchar('t'); 
    U0putchar('i'); 
    U0putchar('o'); 
    U0putchar('n'); 
    U0putchar('e'); 
    U0putchar('d'); 
    U0putchar(' '); 
    U0putchar('t'); 
    U0putchar('o');
    U0putchar(' '); 
    U0putchar('D'); 
    U0putchar('i'); 
    U0putchar('s'); 
    U0putchar('a'); 
    U0putchar('b'); 
    U0putchar('l'); 
    U0putchar('e'); 
    U0putchar('d'); 
    U0putchar(' '); 
    U0putchar('S');
    U0putchar('t'); 
    U0putchar('a');
    U0putchar('t'); 
    U0putchar('e'); 
    U0putchar(' '); 
    U0putchar('a'); 
    U0putchar('t'); 
    U0putchar(':'); 
    U0putchar(' '); 
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();
    int second = now.second();
    unsigned char hourT, hourO, minuteT, minuteO, secondT, secondO;
    hourT = (hour/10)%10;
    hourT = hourT +'0';
    hourO = (hour/1)%10;
    hourO = hourO + '0';
    minuteT = (minute/10)%10;
    minuteT = minuteT +'0';
    minuteO = (minute/1)%10;
    minuteO = minuteO + '0';
    secondT = (second/10)%10;
    secondT = secondT +'0';
    secondO = (second/1)%10;
    secondO = secondO + '0';
    U0putchar(hourT); 
    U0putchar(hourO); 
    U0putchar(':'); 
    U0putchar(minuteT); 
    U0putchar(minuteO); 
    U0putchar(':'); 
    U0putchar(secondT); 
    U0putchar(secondO); 
    U0putchar('\n'); 
    state_change_B = state_change_A;
  }

  if(!timer_running){
    // start the timer
    *myTCCR1B |= 0x01;
    // set the running flag
    timer_running = 1;
  }

  //monitor start button
  if(*pin_b & 0x10)
    start_flag = 1;
  
  //-----turn on yellow LED
  *port_g |= 0x02;
}

void exit_Disabled(){
  state_change_A = 0;
  if(state_change_A != state_change_B){
    state_change_B = state_change_A;
  }
  //-----turn off yellow LED
  *port_g &= 0xFD;
  //turn off start button flag
  start_flag = 0;
}

void enter_Idle(){
  //display humidity and temp to lcd screen
  float humi  = dht.readHumidity();
  float tempF = dht.readTemperature(true);
  if(i == 10000){
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("Temp: ");
    lcd.print(tempF);
    lcd.print((char)223);
    lcd.print("F");
    lcd.setCursor(0,1);
    lcd.print("Humidity: ");
    lcd.print(humi);
    lcd.print("%");
    delay(1000);
    i = 0;
  }
  //print transition time to serial monitor
  state_change_A = 1;
  if(state_change_A != state_change_B){
    U0putchar('T'); 
    U0putchar('r');
    U0putchar('a'); 
    U0putchar('n'); 
    U0putchar('s'); 
    U0putchar('i'); 
    U0putchar('t'); 
    U0putchar('i'); 
    U0putchar('o'); 
    U0putchar('n'); 
    U0putchar('e'); 
    U0putchar('d'); 
    U0putchar(' '); 
    U0putchar('t'); 
    U0putchar('o');
    U0putchar(' '); 
    U0putchar('I'); 
    U0putchar('d'); 
    U0putchar('l'); 
    U0putchar('e'); 
    U0putchar(' '); 
    U0putchar('S');
    U0putchar('t'); 
    U0putchar('a');
    U0putchar('t'); 
    U0putchar('e'); 
    U0putchar(' '); 
    U0putchar('a'); 
    U0putchar('t'); 
    U0putchar(':'); 
    U0putchar(' '); 
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();
    int second = now.second();
    unsigned char hourT, hourO, minuteT, minuteO, secondT, secondO;
    hourT = (hour/10)%10;
    hourT = hourT +'0';
    hourO = (hour/1)%10;
    hourO = hourO + '0';
    minuteT = (minute/10)%10;
    minuteT = minuteT +'0';
    minuteO = (minute/1)%10;
    minuteO = minuteO + '0';
    secondT = (second/10)%10;
    secondT = secondT +'0';
    secondO = (second/1)%10;
    secondO = secondO + '0';
    U0putchar(hourT); 
    U0putchar(hourO); 
    U0putchar(':'); 
    U0putchar(minuteT); 
    U0putchar(minuteO); 
    U0putchar(':'); 
    U0putchar(secondT); 
    U0putchar(secondO); 
    U0putchar('\n'); 
    state_change_B = state_change_A;
  }
  //if water is too low set water level flag
  *port_b |= 0x02;
  int water_lvl = analogRead(water_lvl_signal_pin);
  if(water_lvl <= water_lvl_threshold)
    water_lvl_flag = 1;

  //if temp is above threshold set temp flag
  if(tempF > temp_threshold)
    temp_flag = 1;
  
  //-----turn on green LED
  *port_d |= 0x80;

  //monitor stop button
  if(*pin_b & 0x20)
    stop_flag = 1;

  i++;
}

void exit_Idle(){
  state_change_A = 0;
  if(state_change_A != state_change_B){
    state_change_B = state_change_A;
  }

  //reset water level flag
  water_lvl_flag = 0;

  //stop monitoring stop buttom
  stop_flag = 0;
  temp_flag = 0;

  //-----turn off green LED
  *port_d &= 0x7F;
}

void enter_Error(){
  //display error message "Water level is too low" on LDC
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Water level is");
  lcd.setCursor(0,1);
  lcd.print("too low");
  state_change_A = 1;
  if(state_change_A != state_change_B){
    U0putchar('T'); 
    U0putchar('r');
    U0putchar('a'); 
    U0putchar('n'); 
    U0putchar('s'); 
    U0putchar('i'); 
    U0putchar('t'); 
    U0putchar('i'); 
    U0putchar('o'); 
    U0putchar('n'); 
    U0putchar('e'); 
    U0putchar('d'); 
    U0putchar(' '); 
    U0putchar('t'); 
    U0putchar('o');
    U0putchar(' '); 
    U0putchar('E'); 
    U0putchar('r'); 
    U0putchar('r'); 
    U0putchar('o'); 
    U0putchar('r'); 
    U0putchar(' '); 
    U0putchar('S');
    U0putchar('t'); 
    U0putchar('a');
    U0putchar('t'); 
    U0putchar('e'); 
    U0putchar(' '); 
    U0putchar('a'); 
    U0putchar('t'); 
    U0putchar(':'); 
    U0putchar(' '); 
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();
    int second = now.second();
    unsigned char hourT, hourO, minuteT, minuteO, secondT, secondO;
    hourT = (hour/10)%10;
    hourT = hourT +'0';
    hourO = (hour/1)%10;
    hourO = hourO + '0';
    minuteT = (minute/10)%10;
    minuteT = minuteT +'0';
    minuteO = (minute/1)%10;
    minuteO = minuteO + '0';
    secondT = (second/10)%10;
    secondT = secondT +'0';
    secondO = (second/1)%10;
    secondO = secondO + '0';
    U0putchar(hourT); 
    U0putchar(hourO); 
    U0putchar(':'); 
    U0putchar(minuteT); 
    U0putchar(minuteO); 
    U0putchar(':'); 
    U0putchar(secondT); 
    U0putchar(secondO); 
    U0putchar('\n'); 
    state_change_B = state_change_A;
  }

  //-----turn on red LED
  *port_c |= 0x80;

  //ensure motor is set to off
  *port_l_E &= 0xFD;
  //monitor reset button
  if(*pin_b & 0x40)
    reset_flag = 1;
  //monitor stop button
  if(*pin_b & 0x20)
    stop_flag = 1;
  i++;
}

void exit_Error(){
  state_change_A = 0;
  if(state_change_A != state_change_B){
    state_change_B = state_change_A;
  }
  //-----turn off red LED
  *port_c &= 0xF7;

  //stop monitoring stop button
  reset_flag = 0;
  //stop monitoring reset button
  stop_flag = 0;
}

void enter_Running(){
  float humi  = dht.readHumidity();
  float tempF = dht.readTemperature(true);
  int water_lvl = analogRead(water_lvl_signal_pin);
  if(i == 10000){
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("Temp: ");
    lcd.print(tempF);
    lcd.print((char)223);
    lcd.print("F");
    lcd.setCursor(0,1);
    lcd.print("Humidity: ");
    lcd.print(humi);
    lcd.print("%");
    delay(1000);
    i = 0;
  }
  state_change_A = 1;
  if(state_change_A != state_change_B){
    U0putchar('T'); 
    U0putchar('r');
    U0putchar('a'); 
    U0putchar('n'); 
    U0putchar('s'); 
    U0putchar('i'); 
    U0putchar('t'); 
    U0putchar('i'); 
    U0putchar('o'); 
    U0putchar('n'); 
    U0putchar('e'); 
    U0putchar('d'); 
    U0putchar(' '); 
    U0putchar('t'); 
    U0putchar('o');
    U0putchar(' '); 
    U0putchar('R'); 
    U0putchar('u'); 
    U0putchar('n'); 
    U0putchar('n'); 
    U0putchar('i'); 
    U0putchar('n'); 
    U0putchar('g'); 
    U0putchar(' '); 
    U0putchar('S');
    U0putchar('t'); 
    U0putchar('a');
    U0putchar('t'); 
    U0putchar('e'); 
    U0putchar(' '); 
    U0putchar('a'); 
    U0putchar('t'); 
    U0putchar(':'); 
    U0putchar(' '); 
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();
    int second = now.second();
    unsigned char hourT, hourO, minuteT, minuteO, secondT, secondO;
    hourT = (hour/10)%10;
    hourT = hourT +'0';
    hourO = (hour/1)%10;
    hourO = hourO + '0';
    minuteT = (minute/10)%10;
    minuteT = minuteT +'0';
    minuteO = (minute/1)%10;
    minuteO = minuteO + '0';
    secondT = (second/10)%10;
    secondT = secondT +'0';
    secondO = (second/1)%10;
    secondO = secondO + '0';
    U0putchar(hourT); 
    U0putchar(hourO); 
    U0putchar(':'); 
    U0putchar(minuteT); 
    U0putchar(minuteO); 
    U0putchar(':'); 
    U0putchar(secondT); 
    U0putchar(secondO); 
    U0putchar('\n'); 
    state_change_B = state_change_A;
  }

  //if temp drops below threshold reset temp flag
  if(tempF <= temp_threshold)
    temp_flag = 1;

  //if water level gets too low set water level flag
  if(water_lvl < water_lvl_threshold)
    water_lvl_flag = 1;

  //start motor
  *port_l_E |= 0x02;
  *port_l_A |= 0x08;
  *port_l_B &= 0xDF;

  //-----turn on blue LED
  *port_c |= 0x20;

  //monitor stop button
  if(*pin_b & 0x20)
    stop_flag = 1;
  i++;
}

void exit_Running(){
  state_change_A = 0;
  if(state_change_A != state_change_B){
    state_change_B = state_change_A;
  }
  //stop motor
  *port_l_E &= 0xFD;

  //-----turn off blue LED
  *port_c &= 0xDF;
  
  //reset all flags
  temp_flag = 0;
  water_lvl_flag = 0;
  stop_flag = 0;
}

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

// Timer setup function
void setup_timer_regs()
{
  // setup the timer control registers
  *myTCCR1A= 0x00;
  *myTCCR1B= 0X00;
  *myTCCR1C= 0x00;
  
  // reset the TOV flag
  *myTIFR1 |= 0x01;
  
  // enable the TOV interrupt
  *myTIMSK1 |= 0x01;
}


// TIMER OVERFLOW ISR
ISR(TIMER1_OVF_vect)
{
  *myTCCR1B &=0xF8;
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  *myTCCR1B |= 0x01;

}


