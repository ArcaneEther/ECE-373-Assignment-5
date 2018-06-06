/*
  ECE 373
  Instructors PJ Waskiewicz Jr. and Shannon Nelson
  Higgins, Jeremy
  Assignment #5 - PCI Userspace.
  License: GPU.
  
  The purpose of this program is to write a user-space program to
  manipulate the LEDs on an e1000 network device.
  
  Modified from Intel Corporation's original, Copyright(c) 2009.
  Original GNU license below.
*/

/*
  Copyright(c) 2009 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  e1000 mailing list <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
*/


/* Required Libraries. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pci/pci.h>
#include <stdint.h>
#include <linux/types.h>


/* Defines. */
#define MEMORY_WINDOW_SIZE  0x0200000
#define LEDCTL              0x00E00
#define GOOD_PACKETS        0x04074
#define DELAY               1

/* LED Cycle. */
static int led_cycle(struct pci_dev *dev){
  /* Local Variables. */
  int dev_mem_fd;        /* The File Descriptor of the Exported Kernel Memory. */
  int i;                 /* Sentry variable for a For loop. */
  volatile void *memory; /* The mapped Kernel Memory. */
  u32 original;          /* The default value of a register. */
  u32 current;           /* The current value of a register. Used for RMW. */
  
  /* Open Exported Kernel Memory. */
  dev_mem_fd = open("/dev/mem", O_RDWR);
  
  /* File open failed. */
  if(dev_mem_fd < 0){
    perror("Open: ");
    return(-1);
  }
  
  /* Map Kernel memory to userspace. */
  memory = mmap(NULL, *dev->size, (PROT_READ | PROT_WRITE), MAP_SHARED, dev_mem_fd, *dev->base_addr);
  
  /* Memory mapping failed. */
  if(memory == MAP_FAILED){
    perror("mmap/readable - try rebooting with iomem=relaxed.\n");
    close(dev_mem_fd);
    return(-1);
  }
  
  /* Print the status of the LED register. */
  printf("LED Status: 0x%x\n", *((u32 *)(memory + LEDCTL)));
  
  /* Save the status of the LED register. */
  original = *((u32 *)(memory + LEDCTL));
  
  /* Turn the LED2 and LED0 LEDs on for 2 seconds. */
  current = *((u32 *)(memory + LEDCTL));         /* Read. */
  current = (current & 0xFFF0FFF0) | 0x000E000E; /* Modify. */
  *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
  sleep(DELAY * 2);

  /* Turn all LEDs off for 2 seconds. */
  current = *((u32 *)(memory + LEDCTL));         /* Read. */
  current = (current & 0xF0F0F0F0) | 0x0F0F0F0F; /* Modify. */
  *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
  sleep(DELAY * 2);

  /* Cycle the LEDs 5 times. */
  current = *((u32 *)(memory + LEDCTL));           /* Read. */
  for(i = 0; i < 5; ++i){
    printf("Pass %d\n", (i + 1));
    
    current = (current & 0xF0F0F0F0) | 0x0E0F0F0F; /* Modify - LED3 On. */
    *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
    sleep(DELAY);
    
    current = (current & 0xF0F0F0F0) | 0x0F0E0F0F; /* Modify - LED2 On. */
    *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
    sleep(DELAY);
    
    current = (current & 0xF0F0F0F0) | 0x0F0F0E0F; /* Modify - LED1 On. */
    *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
    sleep(DELAY);
    
    current = (current & 0xF0F0F0F0) | 0x0F0F0F0E; /* Modify - LED0 On. */
    *((u32 *)(memory + LEDCTL)) = current;         /* Write. */
    sleep(DELAY);
  }
  
  /* Restore the original status of the LED register. */
  *((u32 *)(memory + LEDCTL)) = original;
  
  /* Read and print the status of the Good Packets Received Register. */
  current = *((u32 *)(memory + GOOD_PACKETS));
  printf("Good Packets: %d\n", current);
  
  /* Unmap the Kernel Memory. */
  munmap((void *)memory, *dev->size);
  
  /* Close Exported Kernel Memory. */
  close(dev_mem_fd);
  
  /* Return on success. */
  return(0);
}
/* LED Cycle ends. */


/* Main Function. */
int main(int argc, char **argv){
  /* Local Variables. */
  struct pci_access *pacc;
  struct pci_dev *dev;
  
  /* Check if program was called with sudo. */
  if(getuid() != 0){
    printf("This program must be run as root:\n\t$ sudo %s\n", argv[0]);
    exit(1);
  }
  
  /* Get the pci_access structure. */
  pacc = pci_alloc();
  
  /* pci_alloc failed. */
  if(pacc == NULL){
    perror("pci_alloc");
    exit(1);
  }
  
  /* Initialize the PCI library. */
  pci_init(pacc);
  
  /* Get the list of devices. */
  pci_scan_bus(pacc);
  
  /* Iterate over all devices to find the single one we want. */
  for(dev = pacc->devices; dev; dev = dev->next){
    if(dev->vendor_id == PCI_VENDOR_ID_INTEL && dev->device_id == 0x100e){
      break;
    }
  }
  
  /* No Intel devices found. */
  if(!dev){
    printf("No matching Intel device was found.\n");
    pci_cleanup(pacc);
    return(-1);
  }
  
  /* Fill in header info we need. */
  pci_fill_info(dev, (PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_SIZES));
  
  /* Run the LED Cycle. */
  led_cycle(dev);
  
  /* Clean up. */
  pci_cleanup(pacc);
  
  /* Return on success. */
  return(0);
}
/* Main Function ends. */
