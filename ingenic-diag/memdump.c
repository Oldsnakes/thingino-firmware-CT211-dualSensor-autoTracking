/*
	memdump [option] address length
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
		"Usage: memdump [option] start_hex [length_hex] \n"
		"   option: \n"
		"	-d	double word (32 bits), default \n"
		"	-w	word (16 bits) \n"
		"	-b	byte (8 bits) \n"
//		"\n"
	);
}


#define GPIO_BASE		0x10010000

#define CONTROL_REG     0x1300002C // Control register address
#define PAGE_SIZE 4096  // Define a constant for the page size = 0x1000

#define PxDRVL_OFFSET 0x130
#define PxDRVH_OFFSET 0x140

#define BIT_GET(x, n)		(((x) >> (n)) & 1)
#define BIT_SET(x, n)		((x) |= (1 << (n)))
#define BIT_CLR(x, n)		((x) &= ~(1 << (n)))

static void *phys_mem = NULL;
static void *phys_mem1 = NULL;

typedef struct {
	volatile const uint32_t REG[1024];
} XHAL_HandleTypeDef;


union address {
    uint32_t addr_32;
    uint16_t addr_16;
    uint8_t addr_8;
};

static void show_memory(uint32_t mem_start, int32_t len, uint32_t page_start, uint32_t width) {
	int32_t count;
	int32_t remain;
	int32_t skip;
	uint32_t fake_addr = mem_start;
		
	uint32_t offset;
	offset = (mem_start - page_start) / 4;

	volatile XHAL_HandleTypeDef *port = phys_mem;  
// port->REG[offset]

	union address addr;
	
	printf("mem_start = %x, len = %x, page_start = %x, offset = %x\n", mem_start,len, page_start, offset);

	count = len;
	fake_addr & (!0xf);
	skip =  (int32_t) mem_start;
	skip &= 0xf;
	remain = 0x10 - skip;
	remain &= 0xf;
	addr.addr_32 = offset;
	
	int32_t j;
	uint32_t in_dword;
	uint32_t pos;

        if (width == 8) {
	  printf("\e[4mAddress:   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\e[0m\n");
	  while (count > 0) {
		  printf("%08x:",fake_addr);
		  j = 0;
		  while (skip > 0) {
			  printf("   ");
			  j++;
			  fake_addr++;
			  skip--;
		  }
	  
		  while (j < 16) {
			in_dword = port->REG[addr.addr_32 & 0xffffffff];
			pos = j & 0x03;
			if (count > 0) {
				switch (pos) {
					case 0:
						break;
					case 1:
						in_dword = in_dword >> 8;
						break;
					case 2:
						in_dword = in_dword >> 16;
						break;
					case 3:
						in_dword = in_dword >> 24;
			  			addr.addr_32++;
						break;
					default:
						break;
				}

				in_dword &= 0xff;
				printf(" %02x",(in_dword & 0xff));
			  	count--;
			  	fake_addr++;
			  	j++;
			}
		  }
		  printf("\n");
	  }
	}
        if (width == 16) {
	  printf("\e[4mAddress:     0    1    2    3    4    5    6    7\e[0m\n");
	  while (count > 0) {
		  printf("%08x:",fake_addr);
		  j = 0;
		  while (skip > 0) {
			  printf("     ");
			  j++;
			  fake_addr+=2;
			  skip-=2;
		  }
	  
		  while (j < 8) {
			in_dword = port->REG[addr.addr_32 & 0xffffffff];
			if (count > 0) {
				if (j & 0x01) {
				 	printf(" %04x",((in_dword >> 16)& 0xffff));
			  		addr.addr_32++;
				}
				else {
					printf(" %04x",(in_dword & 0xffff));
				}

			  	count-=2;
			  	fake_addr+=2;
			  	j++;
			}
		  }
		  printf("\n");
	  }
	}
        if (width == 32) {
	  printf("\e[4mAddress:         0        1        2        3\e[0m\n");
	  while (count > 0) {
		  printf("%08x:",fake_addr);
		  j = 0;
		  while (skip > 0) {
			  printf("         ");
			  j++;
			  fake_addr+=4;
			  skip-=4;
		  }
	  
		  while (j < 4) {
			  if (count > 0) printf(" %08x",port->REG[addr.addr_32]);
			  count-=4;
			  fake_addr+=4;
			  addr.addr_32++;
//			  offset++;
			  j++;
		  }
		  printf("\n");
	  }
	}
}


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

static long check_val(const char *val) {
	if (!val) {
		printf("error: value not specified");
		exit(2);
	}

	return strtol(val, NULL, 16);
}

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
	uint32_t mem_len = 0x100;
	
	char opt = 'x';
	char *n;
	int32_t i;
	int32_t j;
	int32_t item_width =32;
	char *start_str = NULL;
	char *len_str = NULL;
	
	n = *argv;
	
	if (argc < 2) {
		show_help();
		return 1;
	}

	soc_id();

        for (i=1; i < argc; i++) {
//          printf("argv = %s, 0x%x\n", argv[i],argv[i][0]);
          if (argv[i][0] == '-') {
		if (argc < 3) {
			show_help();
			return (1);
		}
              	opt = argv[i][1];
              	j = i;
              	n++;
          }
	  else {
		if (start_str == NULL) {
		   start_str = argv[i];
		}
		else {		
			if (len_str == NULL) {
		   		len_str = argv[i];
			}
		}
	  }
        } 

	if (opt == 'b') item_width = 8;
	else if (opt == 'w') item_width = 16;

	printf("opt=0x%x, start=%s, len=%s, width=%d\n", opt,start_str,len_str,item_width);

	mem_start = check_val(start_str);
	if ((argc > 3 && opt != 'x') ||
		(argc > 2 && opt == 'x')) {
		mem_len = check_val(len_str);
	} else {
		mem_len = 0x40;
	}
//	printf("Memory Dump:  start:  0x%08x, length:  0x%08x\n", mem_start, mem_len);
	
	int fd = open("/dev/mem", O_RDWR|O_SYNC);

	if (fd < 0) {
		perror("error: failed to open /dev/mem");
		return 2;
	}
//	printf("Memory Dump:  start:  0x%08x, length:  0x%08x\n", mem_start, mem_len);

/*	phys_mem1 = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
	if (phys_mem1 == MAP_FAILED) {
		perror("error: mmap-1 failed");
		return 2;
	}
	printf("Memory Dump:  GPIO map passed.\n");
*/	
	mem_page_start = mem_start;
	mem_page_start &= 0xfffff000;	// align to page 
	phys_mem = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mem_page_start);
	if (phys_mem == MAP_FAILED) {
		perror("error: mmap failed");
		return 2;
	}
	
	printf("Memory Dump:  start:  0x%08x, length:  0x%08x\n", mem_start, mem_len);

		show_memory(mem_start, mem_len, mem_page_start, item_width);

	return 0;
}
