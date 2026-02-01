/*
	mipisw  0/1
    Copyright (C) 2025

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>

uint32_t read_soc_id(); // Function prototype for read_soc_id

static void show_help() {
	puts(
		"Usage: mipi	[0/1] \n"
//		"\n"
	);
}



#define CONTROL_REG     0x1300002C // Control register address
#define PAGE_SIZE 4096  // Define a constant for the page size

#define BIT_GET(x, n)		(((x) >> (n)) & 1)
#define BIT_SET(x, n)		((x) |= (1 << (n)))
#define BIT_CLR(x, n)		((x) &= ~(1 << (n)))

#define GPIO_BASE		0x10010000
#define GPIO_PortA_PAT0		0x10010040
#define MIPI_SET_REG 		0x10010044
#define MIPI_CLEAR_REG 		0x10010048
#define MIPI_REG_BIT		0x00000080

static void *phys_mem = NULL;

typedef struct {
	uint32_t REG[1024];
} XHAL_HandleTypeDef;

static void memory_write(uint32_t mem_start, int32_t value, uint32_t page_start) {
	uint32_t *pt;
	
	uint32_t offset;
	offset = (mem_start - page_start) / 4;

	volatile XHAL_HandleTypeDef *port = phys_mem;  
	// pt = phys_mem;
	// printf("pt = %p:%x,%x\n",&pt,pt,*pt);

	// printf("%08x:%08x",mem_start,port->REG[offset]);
	port->REG[offset] = value;
	// printf(" -> %08x\n",port->REG[offset]);
}

/*  not needed */
void soc_id() {
    uint32_t soc_id = read_soc_id();
    uint32_t soc_type;

    // Extracting the relevant bits from soc_id to determine the SOC type
    if ((soc_id >> 28) != 1) {
        // For SOC types where upper 4 bits are 1, use bits 12-19
        soc_type = (soc_id >> 12) & 0xFF;
    } else {
        // For other SOC types (like T10/T20), use a different method
        soc_type = ((soc_id << 4) >>
            0x10);
    }
    // Debug
    printf("SOC ID: 0x%08X\n", soc_id);
    printf("SOC Type: 0x%04X\n", soc_type);
}

/*  not needed */
static long check_val(const char *val) {
	if (!val) {
		printf("error: value not specified");
		exit(2);
	}

	return strtol(val, NULL, 16);
}

/*  not needed */
uint32_t read_soc_id() {
    int fd = open("/dev/mem", O_RDONLY);
    if (fd < 0) {
        perror("Error opening /dev/mem");
        return 0;
    }

    void *map_base = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, CONTROL_REG & ~(PAGE_SIZE - 1));
    if (map_base == MAP_FAILED) {
        close(fd);
        perror("Error mapping memory");
        return 0;
    }

    volatile uint32_t *reg = (volatile uint32_t *)(map_base + (CONTROL_REG & (PAGE_SIZE - 1)));
    uint32_t soc_id = *reg;

    munmap(map_base, PAGE_SIZE);
    close(fd);
    return soc_id;
}

int main(int argc, char **argv) {
	
	uint32_t mem_start;
	uint32_t mem_page_start;
	
	char opt = 'x';
	char *n;
	char *start_str = NULL;
	char *value_str = NULL;
	

	mem_start = MIPI_SET_REG;
	if (argc > 1) {
		opt = atoi(argv[1]);
		if (opt == 1) {
			mem_start = MIPI_SET_REG;
		}
		else if (opt == 0) {
			mem_start = MIPI_CLEAR_REG;
		}
	} 
	else {
		return 1;
	}

	// soc_id();

	int fd = open("/dev/mem", O_RDWR|O_SYNC);

	if (fd < 0) {
		perror("error: failed to open /dev/mem");
		return 2;
	}
	mem_page_start = GPIO_BASE;
	mem_page_start &= 0xfffff000;	// align to page 
	phys_mem = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mem_page_start);
	if (phys_mem == MAP_FAILED) {
		perror("error: mmap failed");
		return 2;
	}
	
	memory_write(mem_start, MIPI_REG_BIT, mem_page_start);
    	close(fd);

	return 0;
}
