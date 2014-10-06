/*
 Cyclops.cpp - Cyclops Driver
 Copyright (c) 2014 Jonathan Newman  All right reserved.

*/

#include "Cyclops.h"

// Initialize the CS and analog in look-up tables
static const uint16_t cs_lut_data[] = {(uint16_t)CS0, (uint16_t)CS1, (uint16_t)CS2, (uint16_t)CS3};
const uint16_t *Cyclops::_cs_lut = cs_lut_data;
static const uint16_t a_in_lut_data[] = {(uint16_t)A0, (uint16_t)A1, (uint16_t)A2, (uint16_t)A3};
const uint16_t *Cyclops::_a_in_lut = a_in_lut_data;
static const uint16_t trig_lut_data[] = {(uint16_t)TRIG0, (uint16_t)TRIG1, (uint16_t)TRIG2, (uint16_t)TRIG3};
const uint16_t *Cyclops::_trig_lut = trig_lut_data;

Cyclops::Cyclops(uint16_t channel) {

    // Set the global _channel property
    if (channel != CH0 || channel != CH1 || channel != CH2 || channel != CH3) {
        //Inappropriate channel specification.
        //exit(0);
        // Something that does not screw up serial communication
        // indefinately would be nice here.
    }
    _channel = channel;

    // Set pin modes
    pinMode(_cs_lut[_channel], OUTPUT);
    pinMode(_trig_lut[_channel], INPUT);

    pinMode(WIPER_UD, OUTPUT);
    pinMode(LDAC, OUTPUT);
    pinMode(OC_COMP, OUTPUT);

    // Get the CS and load-dac lines ready
    digitalWrite(_cs_lut[_channel], HIGH);
    digitalWrite(LDAC, HIGH);

    // Initialize the OC_COMP pin
    digitalWrite(OC_COMP, LOW);
	
	// Initialize SPI bus
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.begin();

    // Initialize private variables
	_initialized = true;
}

// Public Accessors
uint16_t Cyclops::get_channel( void ) {
    return _channel;
}

uint16_t Cyclops::get_chip_select_pin( void ) {
    return _cs_lut[_channel];
}

uint16_t Cyclops::get_analog_pin( void ) {
    return _a_in_lut[_channel];
}

uint16_t Cyclops::get_trigger_pin( void ) {
    return _trig_lut[_channel];
}

void Cyclops::over_current_protect( float current_limit_mA) {

    // Measure the current
     float current_mA = measure_current();

    // If we are over-current, then adjust in the input voltage divider
    if (current_mA > current_limit_mA) {

        // Signal that we are in compenstation mode
        digitalWrite(OC_COMP, HIGH);

        // Raise the resistance of the top of the voltage divder
        mcp4022_decrement_pot(1);
    }
    // If the input stage is not at nominal resistance, then move toward it
    else if (_wiper_position[_channel] > NOM_WIPER_POS-1) {

        // Signal that we are in compenstation mode
        digitalWrite(OC_COMP, HIGH);
        mcp4022_decrement_pot(1);
    }
    // If the input stage is not at nominal resistance, then move toward it
    else if (_wiper_position[_channel] < NOM_WIPER_POS-1) {

        // Signal that we are in compenstation mode
        digitalWrite(OC_COMP, HIGH);
        mcp4022_increment_pot(1);
    }
    else {
        
        // Signal that we are out of compensation mode
        digitalWrite(OC_COMP, LOW);
    }
}

float Cyclops::measure_current(void) {
    
    int sensor_value = analogRead(_a_in_lut[_channel]);
    return 1000.0 * (float)sensor_value * (5.0 / 1023.0);
}

void Cyclops::mcp4921_send_test_waveform(void) {

    uint16_t v_out = 0;

    while (v_out < 4085) {
        v_out += 10;
        mcp4921_load_voltage(v_out);
        mcp4921_load_dac();
    }
    while (v_out > 10) {
        v_out -= 10;
        mcp4921_load_voltage(v_out);
        mcp4921_load_dac();
    }
}


void Cyclops::mcp4921_generate_waveform( int voltage[], uint16_t length, uint16_t sample_period_us) {

    for (int i = 0; i < length; i++) {
        mcp4921_load_voltage(voltage[i]);
        mcp4921_load_dac();
        delayMicroseconds(sample_period_us);  
    }
}

void Cyclops::mcp4921_load_voltage( uint16_t voltage) {

    // Create data packet
    unsigned int spi_out = DAC_CONF_ACTIVE | (voltage & 0x0fff);

    // take the SS pin low to select the chip
    digitalWrite(_cs_lut[_channel], LOW);

    //  send in the address and value via SPI:
    SPI.transfer(spi_out >> 8 & 0xff);
    SPI.transfer(spi_out >> 0 & 0xff);

    // take the SS pin high to de-select the chip:
    digitalWrite(_cs_lut[_channel], HIGH);

}

void Cyclops::mcp4921_load_dac(void) {

    digitalWrite(LDAC, LOW);
    __asm__("nop\n\t");
    __asm__("nop\n\t");
    digitalWrite(LDAC, HIGH);
}

void Cyclops::mcp4921_shutdown_dac (void) {

	// Create data packet
    unsigned int spi_out = DAC_CONF_SHDN;

    // Take the CS pin low to select the chip
    digitalWrite(_cs_lut[_channel], LOW);

    // Send in the config and data via SPI:
    SPI.transfer(spi_out >> 8 & 0xff);
    SPI.transfer(spi_out >> 0 & 0xff);

    // Take the CS pin high to de-select the chip:
    digitalWrite(_cs_lut[_channel], HIGH);

}

void Cyclops::mcp4022_set_nom_AWR (void) {

	// This trimmer is 50k with 64 positions
	// Decrement the wiper 64 times to 0x00
	// ensures that the wiper is connected
	// to terminal B. Incrementing the wiper
	// 64 times ensures that the wiper is
	// connected to terminal A. 
	mcp4022_decrement_pot(64);

	// 5.6k = 50k * x/64. x ~ 7
	mcp4022_increment_pot(NOM_WIPER_POS);

    //_wiper_position[_channel] = NOM_WIPER_POS;

	// Write to EEPROM
	mcp4022_save_pot_resistance();
}

void Cyclops::mcp4022_unpulse_pot(byte n) {

	// Increcment n times
	for (int i = 0; i < n; i++) {
		digitalWrite(WIPER_UD, LOW);
		digitalWrite(WIPER_UD, HIGH);
	}
}

void Cyclops::mcp4022_pulse_pot(byte n) {

	// Decrement n times
	for (int i = 0; i < n; i++) {
		digitalWrite(WIPER_UD, HIGH);
		digitalWrite(WIPER_UD, LOW);
	}
}

void Cyclops::mcp4022_increment_pot( byte n) {
	
	//Bring WIPER_UD HIGH
	digitalWrite(WIPER_UD, HIGH);

    // Take the CS pin low to select the chip
    digitalWrite(_cs_lut[_channel], LOW);

	// Provide n U/D pulses
	mcp4022_unpulse_pot(n);	

    // Increment the wiper position
    if (_wiper_position[_channel] + n >= 63) {
        _wiper_position[_channel] = 63;
    }
    else {
        _wiper_position[_channel] += n;
    }

    // Take the CS pin high to deselect the chip
    digitalWrite(_cs_lut[_channel], HIGH);
}

void Cyclops::mcp4022_decrement_pot( byte n) {
	
	// Bring WIPER_UD LOW
	digitalWrite(WIPER_UD, LOW);

    // Take the CS pin low to select the chip
    digitalWrite(_cs_lut[_channel], LOW);

	// Provide n U/D pulses
	mcp4022_pulse_pot(n);	

    // Decrment the wiper position
    if (_wiper_position[_channel] - n <= 0) {
        _wiper_position[_channel] = 0;
    }
    else {
        _wiper_position[_channel] -= n;
    }

    // Take the CS pin high to deselect the chip
    digitalWrite(_cs_lut[_channel], HIGH);
}

void Cyclops::mcp4022_save_pot_resistance( void ) {

	// Bring WIPER_UD HIGH
	digitalWrite(WIPER_UD, HIGH);

    // Take the CS pin low to select the chip
    digitalWrite(_cs_lut[_channel], LOW);

	// Delay for 10 us
	delayMicroseconds(10);

    // Take the CS pin high to deselect the chip
    digitalWrite(_cs_lut[_channel], HIGH);
}
