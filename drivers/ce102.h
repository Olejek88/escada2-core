//-----------------------------------------------------------------------------
#include <cstring>
#include <cstdint>

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
#define	CURRENT_WS	0x4008
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

#define	WATCH	0x87

#define	CHANNEL_U "29A52371-E9EC-4D1F-8BCB-80F489A96DD3"
#define	CHANNEL_I "E38C561F-9E88-407E-A465-83803A625627"
#define	CHANNEL_W "7BDB38C7-EF93-49D4-8FE3-89F2A2AEDB48"
#define	CHANNEL_F "041DED21-D211-4C0B-BCD6-02E392654332"
#define CHANNEL_T "54051538-38F7-44A3-A9B5-C8B5CD4A2936"

void *ceDeviceThread(void *device);

class DeviceCE;

class DeviceCE {
public:
    uint16_t id;
    char uuid[45];
    char address[10];
    char port[20];
    char dev_time[20];
    uint16_t q_attempt;
    uint16_t q_error;
    uint16_t deviceType;
    uint8_t protocol;
    uint8_t adr;

    // config
    DeviceCE() {
        id = 0;
        strncpy(uuid, "", 40);
        q_attempt = 0;
        q_error = 0;
        protocol = 1;
        adr = 0;
        strncpy(dev_time, "", 20);
        deviceType = 0x1;
        strncpy(address, "1", 10);
        strncpy(port, "/dev/ttyS0", 20);
    }

    bool send_ce(uint16_t op, uint16_t prm, char *request, uint8_t frame);
    uint16_t read_ce(uint8_t *dat, uint8_t type);

    bool ReadInfoCE ();
    int ReadDataCurrentCE();
    int ReadAllArchiveCE(uint16_t tp);
};
