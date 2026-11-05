// Arduino Based EEPROM reader
// Written by Rob Probin, October-November 2026

#include <stdint.h>

//#define SCHEMATIC_VERSION

#ifdef SCHEMATIC_VERSION

#define EEPROM_OE A0
#define EEPROM_CE A1
#define SER_DATA A2
#define SHIFT_CLK_PIN A3    // also know as SRCLK, SCLK, ST_CP .. is the Shift register clock pin.
#define LATCH_PIN A4     // also known as RCLK, SH_CP and is the Storage register clock pin (latch pin)

#else // Prototype 

#define SHIFT_CLK_PIN A0
#define SER_DATA A2
#define EEPROM_CE A1
#define EEPROM_OE A3
#define LATCH_PIN A6

#endif

// Define the data pins
#define DATA(x) (2+x)

// We shift out the address using two 74HC595. 
// We use the Arduino function shiftOut
void write_address(uint16_t address)
{
    // shiftOut(dataPin, clockPin, bitOrder, value)
    // https://docs.arduino.cc/language-reference/en/functions/advanced-io/shiftOut/
    // https://docs.arduino.cc/tutorials/communication/guide-to-shift-out/
    // This page refers to SCLK as SH_CP, and RCLK (also know as SRCLK) ST_CP.
    // RCLK is Storage register clock pin (latch pin)
    // SRCLK is Shift register clock pin.
    byte address_high = address >> 8;
    byte address_low = address & 0xFF;

    digitalWrite(LATCH_PIN, LOW);

    shiftOut(SER_DATA, SHIFT_CLK_PIN, MSBFIRST, address_high);
    shiftOut(SER_DATA, SHIFT_CLK_PIN, MSBFIRST, address_low);

    digitalWrite(LATCH_PIN, HIGH);
}

uint8_t read_data()
{
    byte data = 0;
    for(int data_pin = 7; data_pin >= 0; data_pin--)
    {
        data <<= 1;
        uint8_t pin = DATA(data_pin);
        if(digitalRead(pin))
        {
            data |= 1;
        }
    }
    return data;
}

void setup_ports()
{
    for(int data_pin = 0; data_pin < 8; data_pin++)
    {
        pinMode(DATA(data_pin), INPUT_PULLUP);
    }
    pinMode(EEPROM_OE, OUTPUT); digitalWrite(EEPROM_OE, HIGH);
    pinMode(EEPROM_CE, OUTPUT); digitalWrite(EEPROM_CE, HIGH);
    pinMode(SER_DATA, OUTPUT); 
    pinMode(SHIFT_CLK_PIN, OUTPUT); 
    pinMode(LATCH_PIN, OUTPUT); 

    // LED
    pinMode(13, OUTPUT);    // sets the digital pin 13 as output
}

void read_eeprom(int eeprom_KB)
{
    uint32_t end = eeprom_KB * 1024;
    uint32_t offset = 0;
    if(eeprom_KB == 8)
    {
        // Vpp must be high (A15)
        // PGM must be high (A14)
        // NC doesn't matter (A13)
        offset = 0xC000;
    }
    else if(eeprom_KB == 16)
    {
        // Vpp must be high (A15)
        // PGM must be high (A14)
        offset = 0xC000;
    }
    else if(eeprom_KB == 32)
    {
        // Vpp must be high (A15)
        offset = 0x8000;
    }

    int start = 0;
    for(int address =start; address < end; address++)
    {
        write_address(address + offset);
        digitalWrite(EEPROM_CE, LOW);
        // should be ok to do both at the same time, but Microchip 27C64 data sheet shows OE afterwards
        digitalWrite(EEPROM_OE, LOW);

        delayMicroseconds(5);      // small delay... should be avaiable faster than 250ns
        byte data = read_data();
        if((address & 0xF) == 0)
        {
            Serial.println("");
            Serial.print(offset, HEX); Serial.print(" ");
            Serial.print(address, HEX);
            Serial.print(": ");
        }
        Serial.print(data, HEX); Serial.print(" ");

        digitalWrite(EEPROM_CE, HIGH);
        digitalWrite(EEPROM_OE, HIGH);
    }
    Serial.println("");

}

void setup() {
    setup_ports();
    Serial.begin(115200);
    //Serial.setTimeout(200);   // doesn't seem to work?
    delay(500);
}

void loop() {

    Serial.println("Hello, Welcome to Rob's EEPROM reader");
    Serial.println("");
    Serial.println("1 = 27C64 (8KBytes)");
    Serial.println("2 = 27C128 (16KBytes)");
    Serial.println("3 = 27C256 (32KBytes)");
    Serial.println("4 = 27C512 (64KBytes)");

    int data = 0;
    int led_state = HIGH;
    int led_mode = 0;
    while(1) {
        data = Serial.read(); 
        if(data != -1) {
            break;
        }
        digitalWrite(13, led_state); // sets the digital pin 13 on
        led_state = (led_state == HIGH) ? LOW : HIGH;
        if(led_mode++ < 3)
        {
            delay(100);
        }
        else if(led_mode == 3)
        {
            delay(400);
        }
        else
        {
            delay(400);
            led_mode = 0;
        }
    }

    int eeprom_KB = 0;
    String eeprom_name = "Unknown";
    if (data == '1')
    {
        eeprom_KB = 8;
        eeprom_name = "27C64";
    }
    else if(data == '2')
    {
        eeprom_KB = 16;
        eeprom_name = "27C128";
    }
    else if(data == '3')
    {
        eeprom_KB = 32;
        eeprom_name = "27C256";
    }
    else if(data == '4')
    {
        eeprom_KB = 64;
        eeprom_name = "27C512";
    }
    if(eeprom_KB != 0)
    {
        Serial.print("Reading EEPROM: ");
        Serial.println(eeprom_name);
        read_eeprom(eeprom_KB);
    }
}

