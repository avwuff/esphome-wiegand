#include "esphome.h"


/**
 * Wiegand ESPHOME CUSTOM COMPONENT
 *
 * Modified from https://github.com/monkeyboard/Wiegand-Protocol-Library-for-Arduino
 * Implemented by Luis Millan for RFID reader with wiegand protocol
 */

class WiegandReader : public PollingComponent, public Sensor {

    public:
        WiegandReader(int pinD0, int pinD1): PollingComponent(200), pinD0(pinD0), pinD1(pinD1) { }
        
        void setup() override {
            _lastWiegand = 0;
            _cardTempHigh = 0;
            _cardTemp = 0;
            _code = 0;
            _wiegandType = 0;
            _bitCount = 0;
    
            // Configure the input pins
            pinMode(pinD0, INPUT);
            pinMode(pinD1, INPUT);
    
            // Attach the interrupts
            attachInterrupt(digitalPinToInterrupt(pinD0), ReadD0, FALLING);  // Hardware interrupt - high to low pulse
            attachInterrupt(digitalPinToInterrupt(pinD1), ReadD1, FALLING);  // Hardware interrupt - high to low pulse
        }
    
        void update() override {
            // See if we have a valid code
            noInterrupts();
            bool rc = DoWiegandConversion();
            interrupts();
    
            if(rc) {
                lastCode = millis();
            } else {
                if(keyCodes.length() > 0) {
                    // We have a keyCode, see if the interdigit timer expired
                    if(millis() - lastCode > 2000) {
                        // The interdigit timer expired, send the code and reset for the next string
                        json_message(keyCodes);
                        keyCodes = "";
                    }
                }
            }
        }
    
    private:
        int pinD0;
        int pinD1;
        static volatile unsigned long _cardTempHigh;
        static volatile unsigned long _cardTemp;
        static volatile unsigned long _lastWiegand;
        static volatile int _bitCount;
        static int _wiegandType;
        static unsigned long _code;


        unsigned long lastCode = 0;
        std::string keyCodes = "";

        /**
         * Calls a Home Assistant service with the key code
         * @param keyCode
         */
        void json_message(std::string keyCode) { // Better use json_message2 i have some troubles with .c_str
            ESP_LOGD("custom", "Got a card read");
        }

        void json_message2(unsigned long valueID) {
            ESP_LOGD("custom", "Card read: %ld", valueID);

            // publish this as a new state
            publish_state(valueID);
        }     
        /**
         * D0 Interrupt Handler
         */
        static ICACHE_RAM_ATTR void ReadD0() {
            _bitCount++;				// Increment bit count for Interrupt connected to D0
            if(_bitCount > 31) { 		// If bit count more than 31, process high bits
                _cardTempHigh |= ((0x80000000 & _cardTemp)>>31);	//	shift value to high bits
                _cardTempHigh <<= 1;
                _cardTemp <<=1;
            } else
                _cardTemp <<= 1;		// D0 represent binary 0, so just left shift card data
    
            _lastWiegand = millis();	// Keep track of last wiegand bit received
        }
    
        /**
         * D1 Interrupt Handler
         */
        static ICACHE_RAM_ATTR void ReadD1() {
            _bitCount ++;				// Increment bit count for Interrupt connected to D1
    
            if(_bitCount > 31) {		// If bit count more than 31, process high bits
                _cardTempHigh |= ((0x80000000 & _cardTemp)>>31);	// shift value to high bits
                _cardTempHigh <<= 1;
                _cardTemp |= 1;
                _cardTemp <<=1;
            } else {
                _cardTemp |= 1;			// D1 represent binary 1, so OR card data with 1 then
                _cardTemp <<= 1;		// left shift card data
            }
            _lastWiegand = millis();	// Keep track of last wiegand bit received
        }
    
        /**
         * Extract the Card ID from the received bit stream
         * @param codehigh
         * @param codelow
         * @param bitlength
         * @return
         */
        unsigned long getCardId(volatile unsigned long *codehigh, volatile unsigned long *codelow, char bitlength) {
            if (bitlength==26)								// EM tag
                return (*codelow & 0x1FFFFFE) >>1;
    
            if (bitlength==34)								// Mifare
            {
                *codehigh = *codehigh & 0x03;				// only need the 2 LSB of the codehigh
                *codehigh <<= 30;							// shift 2 LSB to MSB
                *codelow >>=1;
                return *codehigh | *codelow;
            }
            return *codelow;								// EM tag or Mifare without parity bits
        }
    
        /**
         * Convert the received bitstream
         * @return
         */
        bool DoWiegandConversion () {
            unsigned long cardID;
            unsigned long sysTick = millis();
    
            if ((sysTick - _lastWiegand) > 25)								// if no more signal coming through after 25ms
            {
                    if ((_bitCount==24) || (_bitCount==26) || (_bitCount==32) || (_bitCount==34) || (_bitCount==8) || (_bitCount==4) || (_bitCount==48))  	// bitCount for keypress=4 or 8, Wiegand 26=24 or 26, Wiegand 34=32 or 34
                    {    
                        _cardTemp >>= 1;			// shift right 1 bit to get back the real value - interrupt done 1 left shift in advance
                    
                        if (_bitCount>32)			// bit count more than 32 bits, shift high bits right to make adjustment
                            _cardTempHigh >>= 1;
        
                            // TODO: Handle validation failure case!
                        cardID = getCardId (&_cardTempHigh, &_cardTemp, _bitCount);
                        ESP_LOGD("custom", "Card read high: %ld, low: %ld", _cardTempHigh, _cardTemp);
                        json_message2(cardID);
                        _wiegandType=_bitCount;
                        _bitCount=0;
                        _cardTemp=0;
                        _cardTempHigh=0;
                        _code=cardID;
                        return true;
                    
                    } else {
                    // well time over 25 ms and bitCount !=8 , !=26, !=34 , must be noise or nothing then.
                    _lastWiegand=sysTick;
                    _bitCount=0;
                    _cardTemp=0;
                    _cardTempHigh=0;
                    return false;
                    } 
            }else 
                return false;
            
            
        }
};

volatile unsigned long WiegandReader::_cardTempHigh = 0;
volatile unsigned long WiegandReader::_cardTemp = 0;
volatile unsigned long WiegandReader::_lastWiegand = 0;
volatile int WiegandReader::_bitCount = 0;
unsigned long WiegandReader::_code = 0;
int WiegandReader::_wiegandType = 0;
