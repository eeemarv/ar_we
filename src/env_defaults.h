#ifndef MAC_4_LAST
  #define MAC_4_LAST 0xbe, 0xef, 0x13, 0x01
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_SERVER_IP
#define MQTT_SERVER_IP 192, 168, 0, 70
#endif
#ifndef MQTT_CONNECT_RETRY_TIME
#define MQTT_CONNECT_RETRY_TIME 5000
#endif
#ifndef CLIENT_ID_PREFIX
#define CLIENT_ID_PREFIX "we_"
#endif
#ifndef ETH_RST_PIN
#define ETH_RST_PIN 16
#endif
#ifndef ETH_CS_PIN
#define ETH_CS_PIN 5
#endif
#ifndef SPI_FREQ
#define SPI_FREQ 8000000
#endif
#ifndef W5500
#define ENC29J60
#endif
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
#ifndef OWM_LAT
#define OWM_LAT "51.1968584461"
#endif
#ifndef OWM_LON
#define OWM_LON "4.4625334155"
#endif
#ifndef THINGSPEAK_OWM_INTERVAL_MIN 
#define THINGSPEAK_OWM_INTERVAL_MIN 2
#endif
#ifndef SUB_WATER_TEMP
#define SUB_WATER_TEMP "sens/water_temp"
#endif 
#ifndef PUB_WATER_TEMP
#define PUB_WATER_TEMP "we/water_temp"
#endif
#ifndef PUB_AIR_TEMP
#define PUB_AIR_TEMP "we/air_temp"
#endif
#ifndef PUB_PRESENCE
#define PUB_PRESENCE "we/p"
#endif
#ifndef WATER_TEMP_PUB_INTERVAL
#define WATER_TEMP_PUB_INTERVAL 5000
#endif
#ifndef AIR_TEMP_PUB_INTERVAL
#define AIR_TEMP_PUB_INTERVAL 5000
#endif
#ifndef PRESENCE_INTERVAL
#define PRESENCE_INTERVAL 5000
#endif
#ifndef WATER_TEMP_VALID_TIME
#define WATER_TEMP_VALID_TIME 900000 // 15 min
#endif
#ifndef AIR_TEMP_VALID_TIME
#define AIR_TEMP_VALID_TIME 900000 // 15 min
#endif
#ifndef WATER_TEMP_MIN_SAMPLES
#define WATER_TEMP_MIN_SAMPLES 4
#endif
#ifndef WATER_TEMP_MAX_SAMPLES
#define WATER_TEMP_MAX_SAMPLES 64
#endif
#ifndef NTP_POOL
#define NTP_POOL "pool.ntp.org"
#endif
#ifndef NTP_INTERVAL
#define NTP_INTERVAL 7200000 // 2 hours
#endif
#ifndef NTP_OFFSET
#define NTP_OFFSET 0
#endif
#ifndef TIME_BEFORE_TIMER
#define TIME_BEFORE_TIMER 200
#endif
#ifndef TIME_BEFORE_OWM 
#define TIME_BEFORE_OWM 200
#endif
#ifndef TIME_BEFORE_THING
#define TIME_BEFORE_THING 200
#endif

