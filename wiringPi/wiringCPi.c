#include "wiringPi.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "wiringCPi.h"

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <limits.h>
#include "softPwm.h"
#include "softTone.h"


static int wpimode = -1 ;
#define WPI_MODE_BCM 0
#define WPI_MODE_RAW 1
#define BLOCK_SIZE (4*1024)

#ifdef CONFIG_CLOCKWORKPI_A04

int bcmToGpioCPi[64] =
{
	58,  57,      // 0, 1
	167, 0,      // 2, 3
	1, 2,      // 4  5
	3,  4,      // 6, 7
	5,  6,      // 8, 9
	7,  8,      //10,11
	15,  54,      //12,13
	134,  135,      //14,15

	137, 136,      //16,17
	139,  138,      //18,19
	141,  140,      //20,21
	128,  129,      //22,23
	130,  131,      //24,25
	132, 133,      //26,27
	9,  201,    //28,29
	196,   199,    //30,31

	161,  160,      //32,33
	227,  198,      //34,35
	163, 166,      //36,37
	165,  164,      //38,39
	228,  224,      //40,41
	225, 226,      //42,43
	56,  55,      //44,45
	-1, -1,      //46,47

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// ... 63
};

int CPI_PIN_MASK[8][32] =  //[BANK]  [INDEX]
{
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PC	0
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1,},//PD	32
	{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,},//PE		64
	{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,},//PF		96
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PG	128
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PH	160
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PL	192
	{ 1, 1, 1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,},//PM	224
};

volatile uint32_t *gpio_base;
volatile uint32_t *gpioL_base;

#endif

#ifdef CONFIG_CLOCKWORKPI_A06

int bcmToGpioCPi[64] =
{
	106,  107,      // 0, 1
	104, 10,      // 2, 3
	3, 9,      // 4  5
	4,  90,      // 6, 7
	92,  158,      // 8, 9
	156,  105,      //10,11
	146,  150,      //12,13
	81,  80,      //14,15

	82, 83,      //16,17
	131,  132,      //18,19
	134,  135,      //20,21
	89,  88,      //22,23
	84,  85,      //24,25
	86, 87,      //26,27
	112,  113,    //28,29
	109,   157,    //30,31

	148,  147,      //32,33
	100,  101,      //34,35
	102, 103,      //36,37
	97,  98,      //38,39
	99,  96,      //40,41
	110, 111,      //42,43
	64,  65,      //44,45
	-1, -1,      //46,47

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// ... 63	
};

  int CPI_PIN_MASK[5][32] =  //[BANK]	[INDEX]
  {
   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,},//GPIO0
   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,},//GPIO1
   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,},//GPIO2
   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,},//GPIO3
   { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,},//GPIO4
  };

volatile uint32_t *cru_base;
volatile uint32_t *grf_base;
volatile uint32_t *pmugrf_base;
volatile uint32_t *pmucru_base;
volatile uint32_t *gpio0_base;
volatile uint32_t *gpio1_base;
volatile uint32_t *gpio2_base;
volatile uint32_t *gpio3_base;
volatile uint32_t *gpio4_base;
#endif

#ifdef CONFIG_CLOCKWORKPI_D1

int bcmToGpioCPi[64] =
{
	11,  10,	  // 0, 1
	105, 171, 	 // 2, 3
	170, 178,	   // 4  5
	177,	176,		// 6, 7
	83,	84,		// 8, 9
	12,	97,		//10,11
	98,  99,	  //12,13
	166,  167,		//14,15

	169, 168,	   //16,17
	173,  172,		//18,19
	174,  175,		//20,21
	160,  161,		//22,23
	162,  163,		//24,25
	164, 165,	   //26,27
	113,	112,	//28,29
	111,   110,    //30,31

	8,  9,		//32,33
	109,  108,		//34,35
	107, 106,	   //36,37
	76,  75,		//38,39
	86,  74,		//40,41
	77, 81,	   //42,43
	78,  79,	  //44,45
	-1, -1, 	 //46,47

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// ... 63
};

int CPI_PIN_MASK[6][32] =  //[BANK]  [INDEX]
{
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PB	0
	{ 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PC	32
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PD	64
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,},//PE	96
	{ 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PF	128
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},//PG	160
};

volatile uint32_t *gpio_base;

#endif

static unsigned int readR(unsigned int addr)
{
#ifdef CONFIG_CLOCKWORKPI_A06

	unsigned int val = 0;
	unsigned int mmap_base = (addr & ~MAP_MASK);
	unsigned int mmap_seek = (addr - mmap_base);

	if(mmap_base == CRU_BASE)
		val = *((unsigned int *)((unsigned char *)cru_base + mmap_seek));
	else if(mmap_base == GRF_BASE)
		val = *((unsigned int *)((unsigned char *)grf_base + mmap_seek));
	else if(mmap_base == PMUCRU_BASE)
		val = *((unsigned int *)((unsigned char *)pmucru_base + mmap_seek));
	else if(mmap_base == PMUGRF_BASE)
		val = *((unsigned int *)((unsigned char *)pmugrf_base + mmap_seek));
	else if(mmap_base == GPIO0_BASE)
		val = *((unsigned int *)((unsigned char *)gpio0_base + mmap_seek));
	else if(mmap_base == GPIO1_BASE)
		val = *((unsigned int *)((unsigned char *)gpio1_base + mmap_seek));
	else if(mmap_base == GPIO2_BASE)
		val = *((unsigned int *)((unsigned char *)gpio2_base + mmap_seek));
	else if(mmap_base == GPIO3_BASE)
		val = *((unsigned int *)((unsigned char *)gpio3_base + mmap_seek));
	else if(mmap_base == GPIO4_BASE)
		val = *((unsigned int *)((unsigned char *)gpio4_base + mmap_seek));

	return val;

#elif (defined CONFIG_CLOCKWORKPI_A04)

	uint32_t val = 0;
	uint32_t mmap_base = (addr & ~MAP_MASK);
	uint32_t mmap_seek = ((addr - mmap_base) >> 2);

	if (addr >= GPIOL_BASE)
		val = *(gpioL_base + mmap_seek);
	else
		val = *(gpio_base + mmap_seek);

	return val;

#elif (defined CONFIG_CLOCKWORKPI_D1)

	uint32_t val = 0;
	uint32_t mmap_base = (addr & ~MAP_MASK);
	uint32_t mmap_seek = ((addr - mmap_base) >> 2);

	val = *(gpio_base + mmap_seek);

	return val;

#endif
}

static void writeR(unsigned int val, unsigned int addr)
{
#ifdef CONFIG_CLOCKWORKPI_A06

	unsigned int mmap_base = (addr & ~MAP_MASK);
	unsigned int mmap_seek = (addr - mmap_base);

	if(mmap_base == CRU_BASE) 
		*((unsigned int *)((unsigned char *)cru_base + mmap_seek)) = val;
	else if(mmap_base == GRF_BASE)
		*((unsigned int *)((unsigned char *)grf_base + mmap_seek)) = val;
	else if(mmap_base == PMUCRU_BASE)
		*((unsigned int *)((unsigned char *)pmucru_base + mmap_seek)) = val;
	else if(mmap_base == PMUGRF_BASE)
		*((unsigned int *)((unsigned char *)pmugrf_base + mmap_seek)) = val;
	else if(mmap_base == GPIO0_BASE)
		*((unsigned int *)((unsigned char *)gpio0_base + mmap_seek)) = val;
	else if(mmap_base == GPIO1_BASE)
		*((unsigned int *)((unsigned char *)gpio1_base + mmap_seek)) = val;
	else if(mmap_base == GPIO2_BASE)
		*((unsigned int *)((unsigned char *)gpio2_base + mmap_seek)) = val;
	else if(mmap_base == GPIO3_BASE)
		*((unsigned int *)((unsigned char *)gpio3_base + mmap_seek)) = val;
	else if(mmap_base == GPIO4_BASE)
		*((unsigned int *)((unsigned char *)gpio4_base + mmap_seek)) = val;

#elif (defined CONFIG_CLOCKWORKPI_A04)

	unsigned int mmap_base = (addr & ~MAP_MASK);
	unsigned int mmap_seek = ((addr - mmap_base) >> 2);

	if (addr >= GPIOL_BASE)
		*(gpioL_base + mmap_seek) = val;
	else
		*(gpio_base + mmap_seek) = val;

#elif (defined CONFIG_CLOCKWORKPI_D1)

	unsigned int mmap_base = (addr & ~MAP_MASK);
	unsigned int mmap_seek = ((addr - mmap_base) >> 2);

	*(gpio_base + mmap_seek) = val;

#endif
}

int CPi_get_gpio_mode(int pin)
{
	unsigned int regval = 0;
	unsigned int bank   = pin >> 5;
	unsigned int index  = pin - (bank << 5);
	unsigned int phyaddr = 0;
	unsigned char mode = -1;

	if (CPI_PIN_MASK[bank][index] < 0)
		return -1;
	
#ifdef CONFIG_CLOCKWORKPI_A06

	unsigned int grf_phyaddr = 0, ddr_phyaddr = 0;
	int offset = ((index - ((index >> 3) << 3)));

	if(bank == 0){
		grf_phyaddr = PMUGRF_BASE + ((index >> 3) << 2);
		ddr_phyaddr = GPIO0_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 1){
		grf_phyaddr = PMUGRF_BASE + ((index >> 3) << 2) + 0x10;
		ddr_phyaddr = GPIO1_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}	
	else if(bank == 2){
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2);
		ddr_phyaddr = GPIO2_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 3){
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2) +0x10;
		ddr_phyaddr = GPIO3_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 4){
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2) +0x20;
		ddr_phyaddr = GPIO4_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}

	regval = readR(grf_phyaddr);
	mode = (regval >> (offset << 1)) & 0x3;

	if(mode == 0){
		regval = readR(ddr_phyaddr);
		return (regval >> index) & 1;
	}

	return mode + 1;

#elif (defined CONFIG_CLOCKWORKPI_A04)

	int offset = ((index - ((index >> 3) << 3)) << 2);

	if (bank >= 6) {
		phyaddr = GPIOL_BASE + (bank -6) * 0x24 + ((index >> 3) << 2);
	}
	else {
		phyaddr = GPIO_BASE_MAP + (bank * 0x24) + ((index >> 3) << 2);
	}

	regval = readR(phyaddr);
	mode = (regval >> offset) & 7;

	return mode;

#elif (defined CONFIG_CLOCKWORKPI_D1)

	int offset = ((index - ((index >> 3) << 3)) << 2);

	phyaddr = GPIO_BASE_MAP + (bank * 0x30) + ((index >> 3) << 2);

	regval = readR(phyaddr);
	mode = (regval >> offset) & 0xf;

	return mode;

#endif
}

/*
 * Set GPIO Mode
 */
int CPi_set_gpio_mode(int pin, int mode)
{
    unsigned int regval = 0;
    unsigned int bank   = pin >> 5;
    unsigned int index  = pin - (bank << 5);
    unsigned int phyaddr = 0;

#ifdef CONFIG_CLOCKWORKPI_A04

	int offset = ((index - ((index >> 3) << 3)) << 2);

	if (bank >= 6) {
		phyaddr = GPIOL_BASE + (bank -6) * 0x24 + ((index >> 3) << 2);
	} else {
		phyaddr = GPIO_BASE_MAP + (bank * 0x24) + ((index >> 3) << 2);
	}

#endif

#ifdef CONFIG_CLOCKWORKPI_A06

	int offset = ((index - ((index >> 3) << 3)));
	unsigned int cru_phyaddr, grf_phyaddr, gpio_phyaddr;

	if(bank == 0){
		cru_phyaddr = PMUCRU_BASE + PMUCRU_CLKGATE_CON1_OFFSET;
		grf_phyaddr = PMUGRF_BASE + ((index >> 3) << 2);
		gpio_phyaddr = GPIO0_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 1){
		cru_phyaddr = PMUCRU_BASE + PMUCRU_CLKGATE_CON1_OFFSET;
		grf_phyaddr = PMUGRF_BASE + ((index >> 3) << 2) + 0x10;
		gpio_phyaddr = GPIO1_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 2){
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2);
		gpio_phyaddr = GPIO2_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 3){
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2) +0x10;
		gpio_phyaddr = GPIO3_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}
	else if(bank == 4){
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
		grf_phyaddr = GRF_BASE + ((index >> 3) << 2) +0x20;
		gpio_phyaddr = GPIO4_BASE + GPIO_SWPORTA_DDR_OFFSET;
	}

#endif

#ifdef CONFIG_CLOCKWORKPI_D1
	
	int offset = ((index - ((index >> 3) << 3)) << 2);

	phyaddr = GPIO_BASE_MAP + (bank * 0x30) + ((index >> 3) << 2);
	
#endif

	if (CPI_PIN_MASK[bank][index] != -1) {
#ifndef CONFIG_CLOCKWORKPI_A06
		regval = readR(phyaddr);
		if (wiringPiDebug)
			printf("Before read reg val: 0x%x offset:%d\n",regval,offset);
#endif
		if (wiringPiDebug)
		    printf("Register[%#x]: %#x index:%d\n", phyaddr, regval, index);

		if (INPUT == mode) {
#ifdef CONFIG_CLOCKWORKPI_A06
			writeR(0xffff0180, cru_phyaddr);
			regval = readR(grf_phyaddr);
			regval |= 0x3 << ((offset << 1) | 0x10);
			regval &= ~(0x3 << (offset << 1));
			writeR(regval, grf_phyaddr);
			regval = readR(gpio_phyaddr);
			regval &= ~(1 << index);
			writeR(regval, gpio_phyaddr);
			if (wiringPiDebug){
				regval = readR(gpio_phyaddr);
				printf("Input mode set over reg val: %#x\n",regval);
			}
#else
			regval &= ~(7 << offset);
			writeR(regval, phyaddr);
			regval = readR(phyaddr);
			if (wiringPiDebug)
				printf("Input mode set over reg val: %#x\n",regval);
#endif
		} else if (OUTPUT == mode) {
#ifdef CONFIG_CLOCKWORKPI_A06
			writeR(0xffff0180, cru_phyaddr);
			regval = readR(grf_phyaddr);
			regval |= 0x3 << ((offset << 1) | 0x10);
			regval &= ~(0x3 << (offset << 1));
			writeR(regval, grf_phyaddr);
			regval = readR(gpio_phyaddr);
			regval |= 1 << index;
			writeR(regval, gpio_phyaddr);
			if (wiringPiDebug){
				regval = readR(gpio_phyaddr);
				printf("Out mode get value: 0x%x\n",regval);
			}
#else
			regval &= ~(7 << offset);
			regval |=  (1 << offset);
			if (wiringPiDebug)
				printf("Out mode ready set val: 0x%x\n",regval);
			writeR(regval, phyaddr);
			regval = readR(phyaddr);
			if (wiringPiDebug)
				printf("Out mode get value: 0x%x\n",regval);
#endif
		} else 
			printf("Unknow mode\n");
	} else
		printf("unused pin\n");
}

int CPi_set_gpio_alt(int pin, int mode)
{
#ifdef CONFIG_CLOCKWORKPI_A04
	unsigned int regval = 0;
	unsigned int bank   = pin >> 5;
	unsigned int index  = pin - (bank << 5);
	unsigned int phyaddr = 0;
	int offset = ((index - ((index >> 3) << 3)) << 2);

	if (bank >= 6) {
		phyaddr = GPIOL_BASE + ((index >> 3) << 2);
	}else
		phyaddr = GPIO_BASE_MAP + (bank * 0x24) + ((index >> 3) << 2);

	/* Ignore unused gpio */
	if (CPI_PIN_MASK[bank][index] != -1) {
		if (wiringPiDebug)
			printf("Register[%#x]: %#x index:%d\n", phyaddr, regval, index);

		regval = readR(phyaddr);
		regval &= ~(7 << offset);
		regval |=  (mode << offset);
		writeR(regval, phyaddr);
	} else
		printf("Pin alt mode failed!\n");
#endif

#ifdef CONFIG_CLOCKWORKPI_D1
	unsigned int regval = 0;
	unsigned int bank	= pin >> 5;
	unsigned int index	= pin - (bank << 5);
	unsigned int phyaddr = 0;
	int offset = ((index - ((index >> 3) << 3)) << 2);

	phyaddr = GPIO_BASE_MAP + (bank * 0x30) + ((index >> 3) << 2);

	/* Ignore unused gpio */
	if (CPI_PIN_MASK[bank][index] != -1) {
		if (wiringPiDebug)
			printf("Register[%#x]: %#x index:%d\n", phyaddr, regval, index);

		regval = readR(phyaddr);
		regval &= ~(7 << offset);
		regval |=  (mode << offset);
		writeR(regval, phyaddr);
	} else
		printf("Pin alt mode failed!\n");
#endif

	return 0;
}

/*
 * CPi Digital write 
 */
void CPi_digitalWrite(int pin, int value)
{
	unsigned int bank   = pin >> 5;
	unsigned int index  = pin - (bank << 5);
	unsigned int phyaddr = 0;
	unsigned int regval = 0;

#ifdef CONFIG_CLOCKWORKPI_A04
	
	if (bank >= 6) {
		phyaddr = GPIOL_BASE + (bank -6) * 0x24 + 0x10;
	} else {
		phyaddr = GPIO_BASE_MAP + (bank * 0x24) + 0x10;
	}

#endif

#ifdef CONFIG_CLOCKWORKPI_A06

	unsigned int cru_phyaddr = 0;

	if(bank == 0){
		phyaddr = GPIO0_BASE + GPIO_SWPORTA_DR_OFFSET;
		cru_phyaddr = PMUCRU_BASE + PMUCRU_CLKGATE_CON1_OFFSET;
	}
	else if(bank == 1){
		phyaddr = GPIO1_BASE + GPIO_SWPORTA_DR_OFFSET;
		cru_phyaddr = PMUCRU_BASE + PMUCRU_CLKGATE_CON1_OFFSET;
	}
	else if(bank == 2){
		phyaddr = GPIO2_BASE + GPIO_SWPORTA_DR_OFFSET;			
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
	}
	else if(bank == 3){
		phyaddr = GPIO3_BASE + GPIO_SWPORTA_DR_OFFSET;
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
	}
	else if(bank == 4){
		phyaddr = GPIO4_BASE + GPIO_SWPORTA_DR_OFFSET;			
		cru_phyaddr = CRU_BASE + CRU_CLKGATE_CON31_OFFSET;
	}

#endif

#ifdef CONFIG_CLOCKWORKPI_D1
	
	phyaddr = GPIO_BASE_MAP + (bank * 0x30) + 0x10;

#endif

	if (CPI_PIN_MASK[bank][index] != -1) {

#ifdef CONFIG_CLOCKWORKPI_A06
		writeR(0xffff0180, cru_phyaddr);
#endif

		if (wiringPiDebug)
			printf("pin: %d, bank: %d, index: %d, phyaddr: 0x%x\n", pin, bank, index, phyaddr);

		regval = readR(phyaddr);
		if (wiringPiDebug)
			printf("befor write reg val: 0x%x,index:%d\n", regval, index);
		if(0 == value) {
			regval &= ~(1 << index);
			writeR(regval, phyaddr);
			regval = readR(phyaddr);
			if (wiringPiDebug)
				printf("LOW val set over reg val: 0x%x\n", regval);
		} else {
			regval |= (1 << index);
			writeR(regval, phyaddr);
			regval = readR(phyaddr);
			if (wiringPiDebug)
				printf("HIGH val set over reg val: 0x%x\n", regval);
		}

	} 
}

/*
 * CPi Digital Read
 */
int CPi_digitalRead(int pin)
{
	int bank = pin >> 5;
	int index = pin - (bank << 5);
	int val;
	unsigned int phyaddr;

#ifdef CONFIG_CLOCKWORKPI_A04

	if (bank >= 6) {
		phyaddr = GPIOL_BASE + (bank -6) * 0x24 + 0x10;
	} else {
		phyaddr = GPIO_BASE_MAP + (bank * 0x24) + 0x10;
	}

#endif

#ifdef CONFIG_CLOCKWORKPI_A06

	if(bank == 0)
		phyaddr = GPIO0_BASE + GPIO_EXT_PORTA_OFFSET;
	else if(bank == 1)
		phyaddr = GPIO1_BASE + GPIO_EXT_PORTA_OFFSET;
	else if(bank == 2)
		phyaddr = GPIO2_BASE + GPIO_EXT_PORTA_OFFSET;
	else if(bank == 3)
		phyaddr = GPIO3_BASE + GPIO_EXT_PORTA_OFFSET;
	else if(bank == 4)
		phyaddr = GPIO4_BASE + GPIO_EXT_PORTA_OFFSET;

#endif

#ifdef CONFIG_CLOCKWORKPI_D1
		
	phyaddr = GPIO_BASE_MAP + (bank * 0x30) + 0x10;
	
#endif

	if (CPI_PIN_MASK[bank][index] != -1) {
		val = readR(phyaddr);
		val = val >> index;
		val &= 1;
		if (wiringPiDebug)
			printf("Read reg val: 0x%#x, bank:%d, index:%d phyaddr: 0x%x\n", val, bank, index, phyaddr);
		return val;
	}
	return 0;
}


int CPiSetup(int fd)
{
#ifdef CONFIG_CLOCKWORKPI_A04

	gpio_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIOA_BASE);
	if ((int32_t)(unsigned long)gpio_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO) failed: %s\n", strerror(errno));
	gpioL_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIOL_BASE);
	if ((int32_t)(unsigned long)gpioL_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO) failed: %s\n", strerror(errno));

#elif defined(CONFIG_CLOCKWORKPI_A06)

	gpio0_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO0_BASE);
	if ((int32_t)(unsigned long)gpio0_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO0_BASE) failed: %s\n", strerror(errno));
	gpio1_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO1_BASE);
	if ((int32_t)(unsigned long)grf_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO1_BASE) failed: %s\n", strerror(errno));
	gpio2_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO2_BASE);
	if ((int32_t)(unsigned long)gpio2_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO2_BASE) failed: %s\n", strerror(errno));
	gpio3_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO3_BASE);
	if ((int32_t)(unsigned long)gpio3_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO3_BASE) failed: %s\n", strerror(errno));
	gpio4_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO4_BASE);
	if ((int32_t)(unsigned long)gpio4_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO4_BASE) failed: %s\n", strerror(errno));

	cru_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CRU_BASE);
	if ((int32_t)(unsigned long)cru_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (CRU_BASE) failed: %s\n", strerror(errno));
	pmucru_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PMUCRU_BASE);
	if ((int32_t)(unsigned long)pmucru_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (PMUCRU_BASE) failed: %s\n", strerror(errno));
	grf_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GRF_BASE);
	if ((int32_t)(unsigned long)grf_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GRF_BASE) failed: %s\n", strerror(errno));
	pmugrf_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PMUGRF_BASE);
	if ((int32_t)(unsigned long)pmugrf_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (PMUGRF_BASE) failed: %s\n", strerror(errno));

#elif defined(CONFIG_CLOCKWORKPI_D1)

	gpio_base = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIOA_BASE);
	if ((int32_t)(unsigned long)gpio_base == -1)
		return wiringPiFailure(WPI_ALMOST, "wiringPiSetup: mmap (GPIO) failed: %s\n", strerror(errno));

#endif
	wpimode = WPI_MODE_BCM;
}

void CPiSetupRaw(void)
{
	wpimode = WPI_MODE_RAW;
}

void CPiPinMode(int pin, int mode)
{
	if (wiringPiDebug)
		printf("CPiPinMode: pin:%d,mode:%d\n", pin, mode);

	if (pin >= GPIO_NUM) {
		printf("CPiPinMode: invaild pin:%d\n", pin);
		return;
	}

	if (wpimode == WPI_MODE_BCM) {
		if(pin >= sizeof(bcmToGpioCPi)/sizeof(bcmToGpioCPi[0])) {
			printf("CPiPinMode: invaild pin:%d\n", pin);
			return;
		}
		pin = bcmToGpioCPi[pin];
	}

	CPi_set_gpio_mode(pin, mode);
}

void CPiDigitalWrite(int pin, int value)
{
	if (wiringPiDebug)
		printf("CPiDigitalWrite: pin:%d,value:%d\n", pin, value);

	if (pin >= GPIO_NUM) {
		printf("CPiDigitalWrite: invaild pin:%d\n", pin);
		return;
	}

	if (wpimode == WPI_MODE_BCM) {
		if(pin >= sizeof(bcmToGpioCPi)/sizeof(bcmToGpioCPi[0])) {
			printf("CPiDigitalWrite: invaild pin:%d\n", pin);
			return;
		}
		pin = bcmToGpioCPi[pin];
	}

	CPi_digitalWrite(pin, value);
}

int CPiDigitalRead(int pin)
{
	int value;

	if (pin >= GPIO_NUM) {
		printf("CPiDigitalRead: invaild pin:%d\n", pin);
		return -1;
	}

	if (wpimode == WPI_MODE_BCM) {
		if(pin >= sizeof(bcmToGpioCPi)/sizeof(bcmToGpioCPi[0])) {
			printf("CPiDigitalRead: invaild pin:%d\n", pin);
			return -1;
		}
		pin = bcmToGpioCPi[pin];
	}

	value = CPi_digitalRead(pin);

	if (wiringPiDebug)
		printf("CPiDigitalRead: pin:%d,value:%d\n", pin, value);

	return value;
}

void pinModeAltCP(int pin, int mode)
{
}

void CPiBoardId (int *model, int *rev, int *mem, int *maker, int *warranty)
{
#ifdef CONFIG_CLOCKWORKPI_A04
	*model = CPI_MODEL_A04; 
	*rev = PI_VERSION_1; 
	*mem = 3;
	*maker = 3;
#elif defined(CONFIG_CLOCKWORKPI_A06)
	*model = CPI_MODEL_A06; 
	*rev = PI_VERSION_1; 
	*mem = 4;
	*maker = 3;
#elif defined(CONFIG_CLOCKWORKPI_D1)
	*model = CPI_MODEL_D1; 
	*rev = PI_VERSION_1; 
	*mem = 2;
	*maker = 3;
#endif
}

