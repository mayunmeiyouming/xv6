#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <kern/pci.h>
#include <kern/env.h>
#include <inc/ns.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/x86.h>

volatile void *bar_va;

struct tx_desc
{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct packet
{       
    char body[2048];
};

#define TXRING_LEN 48
#define RDLEN 0x02808
#define RDBAL 0x02800
#define RDBAH 0x02804
#define TDBAL 0x03800
#define TDBAH 0x03804
#define TDLEN 0x03808
#define TDH 0x03810
#define TDT 0x03818
#define TIPG 0x00410
#define TCTL 0x00400
    #define TCTL_EN 0x00000002
    #define TCTL_PSP 0x00000008
    #define TCTL_CT 0x000000ff0
    #define TCTL_COLD 0x007ff000
#define TXD_STATUS_DD 0x01
#define TXD_CMD_RS 0x08
#define TXD_CMD_EOP 0x01
#define TDESC_CMD_RS 0x08
#define TDESC_CMD_EOP 0x01
#define TDESC_STATUS_DD 0x01

#define TX_RECV_ARRAY_LEN 128
#define RAL 0x05400
#define RAH 0x05404
#define MTA_START 0x05200
#define MTA_END 0x053fc
#define IMS 0x000d0
#define RDBAL 0x02800
#define RDBAH 0x02804
#define RDLEN 0x02808
#define RDH 0x02810
#define RDT 0x02818
#define RCTL 0x00100
    #define RCTL_EN 0x00000002
    #define RCTL_BAM 0x00008000
    #define RCTL_BSIZE 0x00030000
    #define RCTL_BSEX 0x04000000            //Buffer Size Extension
    #define RCTL_SECRC 0x08000000           //Strip Ethernet CRC from incoming packet



int attachfn_enable(struct pci_func *pcif);
int try_transmit(physaddr_t addr, size_t len);
int try_receive(struct jif_pkt *jp);

#endif  // SOL >= 6
