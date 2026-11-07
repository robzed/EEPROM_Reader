// Arduino Based EEPROM reader
// Written by Rob Probin, October-November 2026
// MIT license
//
// Copyright (c) 2026 Rob Probin
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation 
// files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, 
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software 
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

#include <stdint.h>
#include <ctype.h>
// ======================================================================================
//
// GPIO Definitions
//

//#define SCHEMATIC_VERSION

#ifdef SCHEMATIC_VERSION

#define EEPROM_OE A0
#define EEPROM_CE A1
#define SER_DATA A2
#define SHIFT_CLK_PIN A3    // also know as SRCLK, SCLK, ST_CP .. is the Shift register clock pin.
#define LATCH_PIN A4     // also known as RCLK, SH_CP and is the Storage register clock pin (latch pin)

#else // Prototype 

#define SHIFT_CLK_PIN A0
#define SER_DATA A1
#define EEPROM_CE A2
#define EEPROM_OE A3
#define LATCH_PIN A6

#endif

// Define the data pins
#define DATA(x) (2+x)

// ======================================================================================
//
// Basic EEPROM reader/writer functions
//

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

void write_byte(uint8_t data)
{
    Serial.print(data>>4, HEX);
    Serial.print(data&0xF, HEX);
}
void write_uint16(uint16_t data)
{
    write_byte(data>>8);
    write_byte(data&0xFF);
}
int data_read_delay_microseconds = 5;  // microseconds

void read_eeprom_block(uint32_t start, uint32_t offset, uint32_t size)
{
    uint32_t end = start+size;
    for(uint32_t address = start; address < end; address++)
    {
        write_address(address+offset);
        digitalWrite(EEPROM_CE, LOW);
        // should be ok to do both at the same time, but Microchip 27C64 data sheet shows OE afterwards
        digitalWrite(EEPROM_OE, LOW);

        delayMicroseconds(data_read_delay_microseconds);      // small delay... should be avaiable faster than 250ns
        byte data = read_data();
        if((address & 0xF) == 0)
        {
            Serial.println("");
            //Serial.print(offset, HEX); Serial.print(" ");
            //Serial.print(address, HEX);
            write_uint16(address);
            Serial.print(": ");
        }
        //Serial.print(data, HEX);
        write_byte(data);
        Serial.print(" ");

        digitalWrite(EEPROM_CE, HIGH);
        digitalWrite(EEPROM_OE, HIGH);
    }
    Serial.println("");

}
void read_eeprom(int eeprom_KB)
{
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

    read_eeprom_block(0, offset, eeprom_KB * 1024);
}

void show_welcome()
{
    Serial.println("Hello, Welcome to Rob's EEPROM reader");
    Serial.println("");
    Serial.println("27C64 = Read 8KBytes EEPROM");
    Serial.println("27C128 = Read 16KBytes EEPROM");
    Serial.println("27C256 = Read 32KBytes EEPROM");
    Serial.println("27C512 = Read 64KBytes EEPROM");
    Serial.println("words = to see other commands");
}

void setup() {

    setup_ports();

    Serial.begin(230400);
    //Serial.setTimeout(200);   // doesn't seem to work?
    delay(500);

    show_welcome();
}

// ======================================================================================
//
// This class flashes the LED when it's waiting for a command
//
class LedFlasher {
    private:
        int led_mode;
        unsigned long led_start_time;
        unsigned long next_time;
    public:
        LedFlasher() : led_mode(0), led_start_time(millis()), next_time(0) { }
        void flash();
        void set_next_time(unsigned long next_milliseconds);
};


void LedFlasher::set_next_time(unsigned long next_milliseconds)
{
    next_time = next_milliseconds;
    led_start_time = millis();
}

void LedFlasher::flash()
{
    if((millis() - led_start_time) > next_time)
    {
        int led_state = LOW;
        switch(led_mode++)
        {
            case 0:
                led_state = HIGH;
                set_next_time(100);
                break;
            case 1:
                led_state = LOW;
                set_next_time(100);
                break;
            case 2:
                led_state = HIGH;
                set_next_time(100);
                break;
            default:
                led_state = LOW;
                set_next_time(800);
                led_mode = 0;
        }
        digitalWrite(13, led_state); // sets the digital pin 13 on
    }
}


// ======================================================================================
//
// This class reads lines and compares commands entered into the command line
//
class LineBuffer {
    private:
#define MAX_CHARS_PER_LINE 132
        char line_buffer[MAX_CHARS_PER_LINE];
        int size;
        char last_cr;
        LedFlasher led_handler;
        const char* word_ptr;
        int word_size;
        void skip_spaces();
    public:
        LineBuffer() : size(0), last_cr(0), word_ptr(line_buffer), word_size(0) { }
        void read_line();
        bool matches_word(const char* string_to_match);
        void get_next_word();
        int get_word_size() { return word_size; }
        bool get_number(int* number_out);
        void print_active_word();
};

bool LineBuffer::matches_word(const char* to_match)
{
    for(int i = 0; i < word_size; i++)
    {
        if(tolower(word_ptr[i]) != tolower(*to_match))
        {
            return false;
        }
        to_match++;
    }
    if(*to_match)   // if not zero terminated, then not a match
    {
        return false;
    }
    return true;
}

//#define DEBUG_WORD_PARSING

void LineBuffer::skip_spaces()
{
#ifdef DEBUG_WORD_PARSING
    if(size)
    {
        Serial.print(" [");
        Serial.print(*word_ptr);
        Serial.print(" ");
        Serial.print((unsigned int)word_ptr);
        Serial.print("] ");
    }
#endif

    while(size && *word_ptr == ' ')
    {
        word_ptr++;
        size--;
    }
}
void LineBuffer::get_next_word()
{
    // skip previous word
    word_ptr += word_size;

#ifdef DEBUG_WORD_PARSING
    Serial.print(" Size1=");
    Serial.print(size);
    Serial.print(" ");
#endif

    skip_spaces();

#ifdef DEBUG_WORD_PARSING
    Serial.print(" Size2=");
    Serial.print(size);
    Serial.print(" ");
#endif

    if(size == 0)
    {
        word_size = 0;
#ifdef DEBUG_WORD_PARSING
        Serial.println(" WS=0* ");
#endif
        return;
    }
    const char *p = word_ptr;
    while(size && *p != ' ')
    {
        p++;
        size--;
    }
    word_size = p - word_ptr;
#ifdef DEBUG_WORD_PARSING
    Serial.print(" WS=");
    Serial.print(word_size);
    Serial.print(" ");
#endif
    print_active_word();

}
void LineBuffer::print_active_word()
{
#ifdef DEBUG_WORD_PARSING
    Serial.print(" {");
    for(int i = 0; i < word_size; i++)
    {
        Serial.print(word_ptr[i]);
    }
    Serial.print("} ");
#endif
}

// return true (success) if it's a number.
bool LineBuffer::get_number(int* number_out)
{
    // $ means hex
    // otherwise decimal
    int base = 10;
    const char *p = word_ptr;
    int ws = word_size;

    if(ws == 0) { return false; }
    if(*p == '$') 
    {
        ws--;
        p++;
        base = 16;
        if(ws == 0) { return false; }
    }

    int number = 0;
    while(ws != 0)
    {
        number *= base;
        char c = *p;
        p++; ws--;
        if(c >= '0' && c <= '9')
        {
            number += c - '0';
        }
        else if(base == 16)
        {
            if(c >= 'a' && c <= 'f')
            {
                number += c - 'a' + 10;
            }
            else if(c >= 'A' && c <= 'F')
            {
                number += c - 'A' + 10;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    *number_out = number;
    return true;
}

void LineBuffer::read_line()
{
    size = 0;
    word_ptr = line_buffer;
    word_size = 0;

    while(1) {
        while(!Serial.available())
        {
            // flash the LED while we are waiting for input
            led_handler.flash();
        }
        int data = Serial.read();
        // process tabs as spaces
        if(data == '\t')
        {
            data = ' ';
        }
        if(data != -1) {
            // <ost serial terminal should send CR when you hit (carriage) return
            // but let's just assume a few might get it wrong, and cope with that.
            // We also take care of the terminals that send CR LF ... insane, but 
            // let's make sure we don't
            if(data == '\n' or data == '\r')
            {
                // we accept either if size of line does not equal zero
                if(size != 0)
                {
                    last_cr = data;
                    return;
                }

                // need to ignore the case when there has been no CR or LF line termination
                if(last_cr == 0)
                {
                    last_cr = data;
                    return;
                }

                // the size is zero - we want to see if it's a LF immediately after a CR or a CR immediately after a LF.
                if(last_cr == data)
                {
                    return;
                }

                // this is a different line end character - so ignore it!                
            }
            else if(data == '\b')
            {
                if(size != 0)
                {
                    Serial.print('\b');
                    Serial.print(' ');
                    Serial.print('\b');
                    size--;
                }
                else
                {
                    Serial.print('\a');
                }
            }
            else if(data >= ' ')
            {
                char c = data;
                Serial.print(c);
                line_buffer[size] = c;
                size++;
                if(size == MAX_CHARS_PER_LINE)
                {
                    return;
                }
            }
            else
            {
                // ignore other characters
            }
        }
    }
}


// ======================================================================================
//
// Basic Stack Implementation
//
class Stack {
    private:
    #define STACK_DEPTH 32
    int _stack[STACK_DEPTH];
    unsigned int index;
    public:
        Stack() : index(0) {}
        int pop();
        void push(int data);
        void list();
};

int Stack::pop()
{
    if(index == 0)
    {
        Serial.println(" *** STACK UNDERFLOW!!! *** ");
        return 0;
    }
    index--;
    return _stack[index];
}

void Stack::push(int data)
{
    if(index==STACK_DEPTH)
    {
        Serial.println(" *** STACK OVERFLOW!!! *** ");
    }
    _stack[index] = data;
    index++;
}
void Stack::list()
{
    Serial.print(" < ");
    for(unsigned i = 0; i < index; i++)
    {
        Serial.print(_stack[i]);
        Serial.print(" ");
    }
    Serial.println("> ");
}


Stack stack;

// ======================================================================================
//
// These are part of the command execution engine
// 
void words_command();
void help_command();

struct func_t {
    const char* name;
    void (*f)(void);
    const char* help;
};


void eeprom_27c64_command()
{
        Serial.println("Reading EEPROM: 27C64");
        read_eeprom(8);
}

void eeprom_27c128_command()
{
        Serial.println("Reading EEPROM: 27C128");
        read_eeprom(16);
}

void eeprom_27c256_command()
{
        Serial.println("Reading EEPROM: 27C256");
        read_eeprom(32);
}

void eeprom_27c512_command()
{
        Serial.println("Reading EEPROM: 27C512");
        read_eeprom(64);
}
void read_data_bus()
{
    uint8_t data = read_data();
    stack.push(data);
}
void write_address_command()
{
    uint16_t address = stack.pop();
    write_address(address);
}

void write_eeprom_oe_command()
{
    int level = stack.pop() ? HIGH : LOW;
    digitalWrite(EEPROM_OE, level);
}
void write_eeprom_cs_command()
{
    int level = stack.pop() ? HIGH : LOW;
    digitalWrite(EEPROM_CE, level);
}
void write_ser_command()
{
    int level = stack.pop() ? HIGH : LOW;
    digitalWrite(SER_DATA, level);
}
void write_clk_command()
{
    int level = stack.pop() ? HIGH : LOW;
    digitalWrite(SHIFT_CLK_PIN, level);
}
void write_latch_command()
{
    int level = stack.pop() ? HIGH : LOW;
    digitalWrite(LATCH_PIN, level);
}
void ms()
{
    delay(stack.pop());
}
void dot_command()
{
    Serial.println(stack.pop());
}
void print_stack_comamnd()
{
    stack.list();
}
void read_block_command()
{
    uint16_t size = stack.pop();
    uint16_t addr = stack.pop();
    read_eeprom_block(addr, 0, size);
}
void data_read_delay_microseconds_command()
{
    data_read_delay_microseconds = stack.pop();
}

func_t words[] = 
{
    { "words", words_command, "Show the list of words" },
    { "help", help_command, "Show the help for the commmand that follows help" },
    { "?", help_command, "Show the help for the commmand that follows help" },
    { "27C64", eeprom_27c64_command, "Read 27C64" },
    { "27C128", eeprom_27c128_command, "Read 27C128" },
    { "27C256", eeprom_27c256_command, "Read 27C256" },
    { "27C512", eeprom_27c512_command, "Read 27C512" },
    { "welcome", show_welcome, "Show welcome message"},
    { "read", read_data_bus, "Read the data bus"},
    { "addr!", write_address_command, "Write address bus"},
    { "oe!" , write_eeprom_oe_command, "Write EEPROM OE Pin"},
    { "cs!", write_eeprom_cs_command, "Write EEPROM CE Pin"},
    { "ser!", write_ser_command, "Write 75HC595 Serial Pin"},
    { "clk!", write_clk_command, "Write 75HC595 Clk Pin"},
    { "latch!", write_latch_command, "{ state -- } Write 75HC595 Latch Pin"},
    { "ms", ms, "{ time -- } Delay milliseconds"},
    { ".", dot_command, "Print number off stack"},
    { ".s", print_stack_comamnd, "Print all numbers of stack"},
    { "read_block", read_block_command, "{ addr size -- } read a block of data"},
    { "read_delay!", data_read_delay_microseconds_command, "{ microseconds -- } set wait after select"}
};


void words_command()
{
    Serial.println("--------------------------------------");
    Serial.println("Commands Available:");
    int number_of_commands = sizeof(words) / sizeof(func_t);
    for(int i = 0; i < number_of_commands; i++)
    {
        Serial.print("  ");
        Serial.print(words[i].name);
        Serial.print(" - ");
        Serial.println(words[i].help);
    }
    Serial.println("--------------------------------------");
}
void help_command()
{
    Serial.println("Not implemented yet - just type 'words'");
}

LineBuffer line;

bool execute_word() // LineBuffer& line)
{
    int number_of_commands = sizeof(words) / sizeof(func_t);
    //Serial.print("    number of commands = ");
    //Serial.println(number_of_commands);

    for(int i = 0; i < number_of_commands; i++)
    {
        //Serial.print("    Checking");
        //Serial.println(words[i].name);
        if(line.matches_word(words[i].name))
        {
            words[i].f();
            return true;
        }
    }
    int number;
    if(line.get_number(&number))
    {
        stack.push(number);
        return true;
    }

    Serial.println("Couldn't find command");
    return false;
}

void loop() {
    
    Serial.println("");
    Serial.print("> ");
    line.read_line();
    Serial.println("");

    line.get_next_word();
    while(line.get_word_size() != 0)
    {
        bool success = execute_word(); // &line);
        if(! success)
        {
            break;
        }
        line.get_next_word();
    }
}

