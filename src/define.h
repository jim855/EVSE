/* RA8875 SPI */
#define LCD_SCK 11
#define LCD_MISO 10
#define LCD_MOSI 9
#define LCD_RST 7
#define LCD_CS 15
#define LCD_INT 16


// ETH
#define ETH_CS 12
#define ETH_RST 14
#define ETH_INT 13
#define ETH_MOSI 5
#define ETH_MISO 6
#define ETH_SCK 48

// Beeper
#define BEEPER 2
#define RESOLUTION 8
#define PWMCHANNEL 0

// LED
#define LED1 40
#define LED2 41
#define LED3 42

/* RC522 & RC532 */
#define RFID_SDA 38
#define RFID_SCL 39

/* UART to ATmega32*/
#define ATMEGA32_RX 18
#define ATMEGA32_TX 17
#define ATMEGA32_BAUD 115200

/*EMBUTTOM*/
#define BUTTON_1 8
#define BUTTON_2 19
#define BUTTON_3 20 

#define DEBUG_PORT Serial
#define RAPI_PORT Serial1

#define POLL_TIME (5 * 1000)

#define EMERGENCY_STOP 20

#define DISCONNECT_INTERVAL 100

struct Setting {
  String name;
  String validTag[10];
};

