//-----------------------------------------------------------------------------
#include <sqltypes.h>
#include <cstring>

#define TYPE_INPUTCE		0xE	    // CE102 type
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

void *mekDeviceThread(void *device);

class DeviceMER;

class DeviceMER {
public:
    u_int16_t id;
    char address[10];
    char port[20];
    u_int16_t q_attempt;
    u_int16_t q_error;
    u_int16_t deviceType;

    // config
    DeviceMER() {
        id = 0;
        q_attempt = 0;
        q_error = 0;
        deviceType = TYPE_MERCURY230;
        strncpy(address, "1", 10);
        strncpy(port, "/dev/ttyS0", 20);
    }

    bool ReadInfo();
    // ReadDataCurrent - read single device connected to concentrator
    int ReadDataCurrent();

    int ReadAllArchive(u_int16_t tp);

    bool send_mercury(u_int8_t op, u_int8_t prm, u_int8_t frame, u_int8_t index);

    u_int16_t read_mercury(u_int8_t *dat, u_int8_t type);

    bool send_ce(u_int8_t op, u_int8_t prm, char *request, u_int8_t frame);

    u_int16_t read_ce(u_int8_t *dat, u_int8_t type);

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
