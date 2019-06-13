//-----------------------------------------------------------------------------
#include <cstring>
#include <stdint.h>

#define TYPE_INPUT_CE		0xE	    // CE102 type
#define TYPE_MERCURY230		0x10	// Mercury 230 type
#define TYPE_SET_4TM		0x11	// Set 4TM type

#define    CHECK        0x0
#define    OPEN_CHANNEL    0x1
#define    CLOSE_CHANNEL    0x2
#define    WRITE_DATA    0x3
#define    READ_TIME_230    0x4
#define    READ_EVENTS    0x4
#define    READ_DATA_230    0x5
#define    READ_POWER    0x6
#define    READ_PARAMETRS    0x8
#define    READ_DATA_230L    0xA
#define    READ_UI        0x11

#define ENERGY_SUM    0x8
#define I1        0x21
#define I2        0x22
#define I3        0x23
#define U1        0x11
#define U2        0x12
#define U3        0x13
#define F        0x40
#define P_SUM        0x00
#define Q_SUM        0x04
#define S_SUM        0x08
//-----------------------------------------------------------------------------
#define START		0x2f
#define STOP		0x21
#define CR		0xD
#define LF		0xA
#define ACK		0x6
#define SOH		0x1
#define STX		0x2
#define ETX		0x3
#define EOT		0x4
#define REQUEST		0x3F

#define	SN		0x5002


#define	CURRENT_U	0x4001
#define	CURRENT_W	0x4003
#define	CURRENT_F	0x400D
#define	CURRENT_I	0x400A

#define	ARCH_MONTH	0x1201
#define	ARCH_DAYS	0x1401
#define	TOTAL_MONTH	0x1101
#define	TOTAL_DAYS	0x1301

#define	READ_DATE	0x10
#define	READ_TIME	0x11

#define	READ_PARAMETERS	0x22
#define	READ_ARCHIVES	0x33
#define	OPEN_CHANNEL_CE	0x50
#define	OPEN_PREV	0x51


void *mekDeviceThread(void *device);

class DeviceMER;

class DeviceMER {
public:
    uint16_t id;
    char uuid[20];
    char address[10];
    char port[20];
    char dev_time[20];
    uint16_t q_attempt;
    uint16_t q_error;
    uint16_t deviceType;
    uint8_t protocol;
    uint8_t adr;

    // config
    DeviceMER() {
        id = 0;
        strncpy(uuid, "", 20);
        q_attempt = 0;
        q_error = 0;
        protocol = 1;
        adr = 0;
        strncpy(dev_time, "", 20);
        deviceType = TYPE_INPUT_CE;
        strncpy(address, "1", 10);
        strncpy(port, "/dev/ttyS0", 20);
    }

    bool ReadInfo();
    // ReadDataCurrent - read single device connected to concentrator
    int ReadDataCurrent();

    int ReadAllArchive(uint16_t tp);

    bool send_mercury(uint8_t op, uint8_t prm, uint8_t frame, uint8_t index);

    uint16_t read_mercury(uint8_t *dat, uint8_t type);

    bool send_ce(uint16_t op, uint16_t prm, char *request, uint8_t frame);

    uint16_t read_ce(uint8_t *dat, uint8_t type);

    // store it to class members and form sequence to send in device
/*
    BYTE *ReadConfig();

    // source data will take from db or class member fields
    bool send_mercury(UINT op);

    // function read data from 2IP
    // reading data will be store to DB and temp data class member fields
    int read_mercury(BYTE *dat);

*/
};
