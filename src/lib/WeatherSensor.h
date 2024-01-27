///////////////////////////////////////////////////////////////////////////////////////////////////
// WeatherSensor.h
//
// Bresser 5-in-1/6-in-1/7-in-1 868 MHz Weather Sensor Radio Receiver
// based on CC1101 or SX1276/RFM95W and ESP32/ESP8266
//
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// NOTE: Application/hardware specific configurations should be made in WeatherSensorCfg.h!
//
// Based on:
// ---------
// Bresser5in1-CC1101 by Sean Siford (https://github.com/seaniefs/Bresser5in1-CC1101)
// RadioLib by Jan Gromeš (https://github.com/jgromes/RadioLib)
// rtl433 by Benjamin Larsson (https://github.com/merbanan/rtl_433)
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c
//     - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_7in1.c
//
// created: 05/2022
//
//
// MIT License
//
// Copyright (c) 2022 Matthias Prinke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// History:
//
// 20220523 Created from https://github.com/matthias-bs/Bresser5in1-CC1101
// 20220524 Moved code to class WeatherSensor
// 20220526 Implemented getData(), changed debug output to macros
// 20220815 Added support of multiple sensors
//          Moved windspeed_ms_to_bft() to WeatherUtils.h/.cpp
// 20221207 Added SENSOR_TYPE_THERMO_HYGRO
// 20220110 Added WEATHER0_RAIN_OV/WEATHER1_RAIN_OV
// 20230228 Added Bresser 7 in 1 decoder by Jorge Navarro-Ortiz (jorgenavarro@ugr.es)
// 20230328 Added MSG_BUF_SIZE
// 20230330 Added changes for Adafruit Feather 32u4 LoRa Radio
// 20230412 Added workaround for Professional Wind Gauge / Anemometer, P/N 7002531
// 20230624 Added Bresser Lightning Sensor decoder
// 20230708 Added SENSOR_TYPE_WEATHER_7IN1 and startup flag
// 20230716 Added decodeMessage() to separate decoding function from receiving function
// 20230723 Added SENSOR_TYPE_WATER
// 20230804 Added Bresser Water Leakage Sensor decoder
// 20231006 Added crc16() from https://github.com/merbanan/rtl_433/blob/master/src/util.c
// 20231024 Added SENSOR_TYPE_POOL_THERMO
// 20231025 Added Bresser Air Quality (Particulate Matter) Sensor, P/N 7009970
//          Modified device type definitions
// 20231030 Refactored sensor data using a union to save memory
// 20231130 Bresser 3-in-1 Professional Wind Gauge / Anemometer, PN 7002531: Replaced workaround 
//          for negative temperatures by fix (6-in-1 decoder)
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef WeatherSensor_h
#define WeatherSensor_h

#include <Arduino.h>
#include <RadioLib.h>


// Sensor Types
// 0 - Weather Station          5-in-1; PN 7002510..12/7902510..12
// 1 - Weather Station          6-in-1; PN 7002585
//   - Professional Wind Gauge  6-in-1; PN 7002531
//   - Weather Station          7-in-1; PN 7003300
// 2 - Thermo-/Hygro-Sensor     6-in-1; PN 7009999
// 3 - Pool / Spa Thermometer   PN 7000073 
// 4 - Soil Moisture Sensor     6-in-1; PN 7009972
// 5 - Water Leakage Sensor     6-in-1; PN 7009975
// 8 - Air Quality Sensor PM2.5/PM10
// 9 - Professional Rain Gauge  (5-in-1 decoder)
// 9 - Lightning Sensor         PN 7009976
// ? - CO2 Sensor
// ? - HCHO/VCO Sensor
#define SENSOR_TYPE_WEATHER0        0 // Weather Station
#define SENSOR_TYPE_WEATHER1        1 // Weather Station
#define SENSOR_TYPE_THERMO_HYGRO    2 // Thermo-/Hygro-Sensor
#define SENSOR_TYPE_POOL_THERMO     3 // Pool / Spa Thermometer
#define SENSOR_TYPE_SOIL            4 // Soil Temperature and Moisture (from 6-in-1 decoder)
#define SENSOR_TYPE_LEAKAGE         5 // Water Leakage
#define SENSOR_TYPE_AIR_PM          8 // Air Quality Sensor (Particle Matter)
#define SENSOR_TYPE_RAIN            9 // Professional Rain Gauge (from 5-in-1 decoder)
#define SENSOR_TYPE_LIGHTNING       9 // Lightning Sensor


// Sensor specific rain gauge overflow threshold (mm)
#define WEATHER0_RAIN_OV          1000
#define WEATHER1_RAIN_OV        100000


// Flags for controlling completion of reception in getData()
#define DATA_COMPLETE           0x1     // only completed slots (as opposed to partially filled)
#define DATA_TYPE               0x2     // at least one slot with specific sensor type
#define DATA_ALL_SLOTS          0x8     // all slots completed

// Message buffer size
#define MSG_BUF_SIZE            27

// Radio message decoding status
typedef enum DecodeStatus {
    DECODE_INVALID, DECODE_OK, DECODE_PAR_ERR, DECODE_CHK_ERR, DECODE_DIG_ERR, DECODE_SKIP, DECODE_FULL
} DecodeStatus;


#if !defined(ARDUINO_ARCH_AVR)
#include <string>

/*!
 * \struct SensorMap
 *
 * \brief Mapping of sensor IDs to names
 */
typedef struct SensorMap {
    uint32_t        id;    //!< ID if sensor (as transmitted in radio message)
    std::string     name;  //!< Name of sensor (e.g. for MQTT topic)
} SensorMap;
#endif


/*!
  \class WeatherSensor

  \brief Receive, decode and store Bresser Weather Sensor Data
  Uses CC1101 or SX1276 radio module for receiving FSK modulated signal at 868 MHz.
*/
class WeatherSensor {
    public:
        /*!
        \brief Constructor.

        */
        /*
        WeatherSensor()
        {
            //memset(this, 0, sizeof(*this));
        };
        */
        /*!
        \brief Presence check and initialization of radio module.

        \returns RADIOLIB_ERR_NONE on success (otherwise does never return).
        */
        int16_t begin(void);


        /*!
        \brief Wait for reception of data or occurrance of timeout.
        With BRESSER_6_IN_1, data is distributed across two different messages. Reception of entire
        data is tried if 'complete' is set.

        \param timeout timeout in ms.

        \param flags    DATA_COMPLETE / DATA_TYPE / DATA_ALL_SLOTS

        \param type     sensor type (combined with FLAGS==DATA_TYPE)

        \param func     Callback function for each loop iteration. (default: NULL)

        \returns false: Timeout occurred.
                 true:  Reception (according to parammeter 'complete') succesful.
        */
        bool    getData(uint32_t timeout, uint8_t flags = 0, uint8_t type = 0, void (*func)() = NULL);


        /*!
        \brief Tries to receive radio message (non-blocking) and to decode it.
        Timeout occurs after a multitude of expected time-on-air.

        \returns DecodeStatus
        */
        DecodeStatus    getMessage(void);

        /*!
        \brief Decode message
        Tries the available decoders until a decoding was successful.

        \returns DecodeStatus
        */
        DecodeStatus    decodeMessage(const uint8_t *msg, uint8_t msgSize);

        struct Weather {
            bool     temp_ok = false;         //!< temperature o.k. (only 6-in-1)
            bool     humidity_ok = false;     //!< humidity o.k.
            bool     light_ok = false;        //!< light o.k. (only 7-in-1)
            bool     uv_ok = false;           //!< uv radiation o.k. (only 6-in-1)
            bool     wind_ok = false;         //!< wind speed/direction o.k. (only 6-in-1)
            bool     rain_ok = false;         //!< rain gauge level o.k.
            float    temp_c = 0.0;            //!< temperature in degC
            float    light_klx = 0.0;         //!< Light KLux (only 7-in-1)
            float    light_lux = 0.0;         //!< Light lux (only 7-in-1)
            float    uv = 0.0;                //!< uv radiation (only 6-in-1)
            float    rain_mm = 0.0;           //!< rain gauge level in mm
            #ifdef WIND_DATA_FLOATINGPOINT   
            float    wind_direction_deg = 0.0;  //!< wind direction in deg
            float    wind_gust_meter_sec = 0.0; //!< wind speed (gusts) in m/s
            float    wind_avg_meter_sec = 0.0;  //!< wind speed (avg)   in m/s
            #endif
            #ifdef WIND_DATA_FIXEDPOINT
            // For LoRa_Serialization:
            //   fixed point integer with 1 decimal -
            //   saves two bytes compared to "RawFloat"
            uint16_t wind_direction_deg_fp1 = 0;  //!< wind direction in deg (fixed point int w. 1 decimal)
            uint16_t wind_gust_meter_sec_fp1 = 0; //!< wind speed (gusts) in m/s (fixed point int w. 1 decimal)
            uint16_t wind_avg_meter_sec_fp1 = 0;  //!< wind speed (avg)   in m/s (fixed point int w. 1 decimal)
            #endif
            uint8_t  humidity = 0;                //!< humidity in %
        };

        struct Soil {
            float    temp_c;                //!< temperature in degC
            uint8_t  moisture;              //!< moisture in % (only 6-in-1)
        };

        struct Lightning {
            uint8_t  distance_km;           //!< lightning distance in km (only lightning)
            uint8_t  strike_count;          //!< lightning strike counter (only lightning)
            uint16_t unknown1;              //!< unknown part 1
            uint16_t unknown2;              //!< unknown part 2

        };

        struct Leakage {
            bool     alarm;                //!< water leakage alarm (only water leakage)
        };

        struct AirPM {
            uint16_t pm_2_5;               //!< air quality PM2.5 in µg/m³
            uint16_t pm_10;                //!< air quality PM10  in µg/m³
        };

        /**
         * \struct Sensor
         *
         * \brief sensor data and status flags
         */
        struct Sensor {
            uint32_t sensor_id;        //!< sensor ID (5-in-1: 1 byte / 6-in-1: 4 bytes / 7-in-1: 2 bytes)
            float    rssi;             //!< received signal strength indicator in dBm
            uint8_t  s_type;           //!< sensor type
            uint8_t  chan;             //!< channel
            bool     startup = false;  //!< startup after reset / battery change
            bool     battery_ok;       //!< battery o.k.
            bool     valid;            //!< data valid (but not necessarily complete)
            bool     complete;         //!< data is split into two separate messages is complete (only 6-in-1 WS)
            union {
                struct Weather      w;
                struct Soil         soil;
                struct Lightning    lgt;
                struct Leakage      leak;
                struct AirPM        pm;
            };

            Sensor ()
            {
                memset(this, 0, sizeof(*this));
            };
        };

        typedef struct Sensor sensor_t;    //!< Shortcut for struct Sensor
        sensor_t sensor[NUM_SENSORS];      //!< sensor data array
        float    rssi = 0.0;               //!< received signal strength indicator in dBm


        /*!
        \brief Generates data otherwise received and decoded from a radio message.

        \returns Always true (for compatibility with getMessage())
        */
        bool    genMessage(int i, uint32_t id = 0xff, uint8_t s_type = 1, uint8_t channel = 0, uint8_t startup = 0);


         /*!
        \brief Clear sensor data

        If 'type' is not specified, all slots are cleared. If 'type' is specified,
        only slots containing data of the given sensor type are cleared.

        \param type Sensor type
        */
        void clearSlots(uint8_t type = 0xFF)
        {
            for (int i=0; i< NUM_SENSORS; i++) {
                if ((type == 0xFF) || (sensor[i].s_type == type)) {
                    sensor[i].valid    = false;
                    sensor[i].complete = false;
                }
            }
        };

        /*!
         * Find slot of required data set by ID
         *
         * \param id    sensor ID
         *
         * \returns     slot (or -1 if not found)
         */
        int findId(uint32_t id);


        /*!
         * Find slot of required data set by type and (optionally) channel
         *
         * \param type      sensor type
         * \param channel   sensor channel (0xFF: don't care)
         *
         * \returns         slot (or -1 if not found)
         */
        int findType(uint8_t type, uint8_t channel = 0xFF);
        
    private:
        struct Sensor *pData; //!< pointer to slot in sensor data array

        /*!
         * \brief Find slot in sensor data array
         *
         * 1. The current sensor ID is checked against the exclude-list (sensor_ids_exc).
         *    If there is a match, the current message is skipped.
         *
         * 2. If the include-list (sensor_ids_inc) is not empty, the current ID is checked
         *    against it. If there is NO match, the current message is skipped.
         *
         * 3. Either an existing slot with the same ID as the current message is updated
         *    or a free slot (if any) is selected.
         *
         * \param id Sensor ID from current message
         *
         * \returns Pointer to slot in sensor data array or NULL if ID is not wanted or
         *          no free slot is available.
         */
        int findSlot(uint32_t id, DecodeStatus * status);


        #ifdef BRESSER_5_IN_1
            /*!
            \brief Decode BRESSER_5_IN_1 message.

            \param msg     Message buffer.

            \param msgSize Message size in bytes.

            \returns Decode status.
            */
            DecodeStatus decodeBresser5In1Payload(const uint8_t *msg, uint8_t msgSize);
        #endif
        #ifdef BRESSER_6_IN_1
            /*!
            \brief Decode BRESSER_6_IN_1 message.
            With BRESSER_6_IN_1, data is distributed across two different messages. Additionally,
            the message format supports different kinds of sensors.

            \param msg     Message buffer.

            \param msgSize Message size in bytes.

            \returns Decode status.
            */
            DecodeStatus decodeBresser6In1Payload(const uint8_t *msg, uint8_t msgSize);
        #endif
        #ifdef BRESSER_7_IN_1
            /*!
            \brief Decode BRESSER_7_IN_1 message.

            \param msg     Message buffer.

            \param msgSize Message size in bytes.

            \returns Decode status.
            */
            DecodeStatus decodeBresser7In1Payload(const uint8_t *msg, uint8_t msgSize);
        #endif
        #ifdef BRESSER_LIGHTNING
             /*!
            \brief Decode BRESSER_LIGHTNING message. (similar to 7-in-1)

            \param msg     Message buffer.

            \param msgSize Message size in bytes.

            \returns Decode status.
            */
           DecodeStatus decodeBresserLightningPayload(const uint8_t *msg, uint8_t msgSize);
        #endif
        #ifdef BRESSER_LEAKAGE
             /*!
            \brief Decode BRESSER_LEAKAGE message. (similar to 7-in-1)

            \param msg     Message buffer.

            \param msgSize Message size in bytes.

            \returns Decode status.
            */
           DecodeStatus decodeBresserLeakagePayload(const uint8_t *msg, uint8_t msgSize);
        #endif

    protected:
        /*!
        \brief Linear Feedback Shift Register - Digest16 (Data integrity check).
        */
        uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key);

        /*!
        \brief Calculate sum of all message bytes.

        \param message   Message buffer.
        \param num_bytes Number of bytes.

        \returns Sum of all message bytes.
        */
        int add_bytes(uint8_t const message[], unsigned num_bytes);

        /*!
        \brief Calculate crc16 of all message bytes.

        \param message      Message buffer.
        \param num_bytes    Number of bytes.
        \param polynomial   Polynomial
        \param initial      Initial value.

        \returns CRC16 of all message bytes.
        */
        uint16_t crc16(uint8_t const message[], unsigned nBytes, uint16_t polynomial, uint16_t init);

        #if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
            /*!
             * \brief Log message payload
             *
             * \param descr    Description.
             * \param msg      Message buffer.
             * \param msgSize  Message size.
             *
             * Result (example):
             *  Byte #: 00 01 02 03...
             * <descr>: DE AD BE EF...
             */
            void log_message(const char *descr, const uint8_t *msg, uint8_t msgSize) {
                char buf[128];
                const char txt[] = "Byte #: ";
                int offs;
                int len1 = strlen(txt);
                int len2 = strlen(descr) + 2; // add colon and space
                int prefix_len = max(len1, len2);
    
                memset(buf, ' ', prefix_len);
                buf[prefix_len] = '\0';
                offs = (len1 < len2) ? (len2 - len1) : 0;
                strcpy(&buf[offs], txt);
              
                // Print byte index
                for (size_t i = 0 ; i < msgSize; i++) {
                    sprintf(&buf[strlen(buf)], "%02d ", i);
                }
                log_d("%s", buf);
          
                memset(buf, ' ', prefix_len);
                buf[prefix_len] ='\0';
                offs = (len1 > len2) ? (len1 - len2) : 0;
                sprintf(&buf[offs], "%s: ", descr);
              
                for (size_t i = 0 ; i < msgSize; i++) {
                    sprintf(&buf[strlen(buf)], "%02X ", msg[i]);
                }
                log_d("%s", buf);
            }
        #endif

};

#endif
