#include <kern/e1000.h>
#include <kern/pmap.h>

struct tx_desc tx_send[TXRING_LEN] __attribute__((aligned(PGSIZE)));
struct tx_desc tx_receive[TX_RECV_ARRAY_LEN] __attribute__((aligned(PGSIZE)));
//struct packet pbuf[TXRING_LEN] __attribute__((aligned (PGSIZE)));

// LAB 6: Your driver code here
int attachfn_enable(struct pci_func *pcif) {
    pci_func_enable(pcif);
   
    for(int i = 0; i < TXRING_LEN; i++){
        memset(&tx_send[i], 0, sizeof(tx_send[i]));
        //tx_d[i].addr = PADDR(&(pbuf[i]));
        tx_send[i].status = TXD_STATUS_DD;
        tx_send[i].cmd = TXD_CMD_RS | TXD_CMD_EOP;
    }

    //为reg_base的物理地址映射虚拟内存
    bar_va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]); 
    uint32_t *status_reg = (uint32_t *)(bar_va + 0x8); 
	//查看状态寄存器的值是否是0x80080783
    assert(*status_reg == 0x80080783);

    // send
    *(volatile uint32_t *)(bar_va + TDBAL) = PADDR(tx_send);
    *(volatile uint32_t *)(bar_va + TDBAH) = 0;
    *(volatile uint32_t *)(bar_va + TDLEN) = TXRING_LEN * sizeof(struct tx_desc);
    *(volatile uint32_t *)(bar_va + TDH) = 0;
    *(volatile uint32_t *)(bar_va + TDT) = 0;
    uint32_t tctl = TCTL_EN | TCTL_PSP | (TCTL_CT & (0x10 << 4)) | (TCTL_COLD & (0x40 << 12));
    *(volatile uint32_t *)(bar_va + TCTL) = tctl;
    *(volatile uint32_t *)(bar_va + TIPG) = 10 | (8 << 10) | (12 << 20);

    //receive
    *(volatile uint32_t *) (bar_va + RAL) = 0x12005452;
	uint32_t rah = 0x5634 | (1u << 31);
	*(volatile uint32_t *) (bar_va + RAH) = rah;
	for(volatile void *c = bar_va + MTA_START; c < bar_va + MTA_END; ++c)
		*(volatile char *) c = 0;                                // MTA
	*(volatile uint32_t *) (bar_va + IMS) = 0;                   // IMS
	*(volatile uint32_t *) (bar_va + RDBAL) = PADDR(tx_receive);// RDBAL
	*(volatile uint32_t *) (bar_va + RDBAH) = 0;                // RDBAH
	*(volatile uint32_t *) (bar_va + RDLEN) = TX_RECV_ARRAY_LEN * sizeof(struct tx_desc);// RDLEN
	*(volatile uint32_t *) (bar_va + RDH) = 0;                // RDH
	*(volatile uint32_t *) (bar_va + RDT) = TX_RECV_ARRAY_LEN - 1;       // RDT
	for(int i = 0; i < TX_RECV_ARRAY_LEN; i++)
	{
		struct PageInfo *pp = page_alloc(0);
		if(!pp)
			panic("fail to alloc memory for e1000 receiver\n");
		tx_receive[i].addr = page2pa(pp);
	}
	uint32_t rctl = (RCTL_EN) | (RCTL_BAM) | (RCTL_BSIZE) | (RCTL_BSEX) | (RCTL_SECRC);
	*(volatile uint32_t *) (bar_va + RCTL) = rctl;        // RCTL

    return 0;
}

int try_transmit(physaddr_t pa, size_t len)
{
    uint32_t tdt = *(volatile uint32_t *) (bar_va + TDT);
	if(tdt + 1 == TXRING_LEN && !(tx_send[(tdt + 1) % TXRING_LEN].status & 1))
		return -1;

    tx_send[tdt].addr = pa;
    tx_send[tdt].length = len;
    tx_send[tdt].status &= ~TDESC_STATUS_DD;
    tx_send[tdt].cmd |= (TDESC_CMD_EOP | TDESC_CMD_RS);
    tdt = (tdt + 1) % TXRING_LEN;
    *(volatile uint32_t *) (bar_va + TDT) = tdt;
    return 0;
}

int try_receive(struct jif_pkt *jp)
{
	uint32_t rdt = *(volatile uint32_t *) (bar_va + RDT);
	volatile struct tx_desc *rd = tx_receive + (rdt + 1) % TX_RECV_ARRAY_LEN;
	if(!(rd->status & 1))
		return -1;
	pte_t *pte;
	lcr3(PADDR(curenv->env_pgdir));
	jp->jp_len = rd->length;
	page_insert(curenv->env_pgdir, pa2page(rd->addr), NULL, PTE_P | PTE_W);
	memcpy(jp->jp_data, NULL, jp->jp_len);
	page_remove(curenv->env_pgdir, NULL);
	lcr3(PADDR(kern_pgdir));
	rdt = (rdt + 1) % TX_RECV_ARRAY_LEN;
	*(volatile uint32_t *) (bar_va + RDT) = rdt;
	return 0;
}