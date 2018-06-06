/*******************************************************************************
 
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

*******************************************************************************/

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

#define MEM_WINDOW_SZ  0x0200000

static struct pci_filter filter;    /* Device filter */

static struct option opts[] = {
	{"write", 1, NULL, 'w' },
	{"address", 1, NULL, 'a' },
	{"debug", 0, NULL, 'D' },
	{"device", 1, NULL, 'd' },
	{"slot", 1, NULL, 's' },
	{ 0, 0, NULL, '0' }
};

static void usage(char *progname, char *idfile)
{
	printf(	"Usage: %s [options] [device]   (%s)\n\n"
		"Options:\n"
		"-w <value>\t\tValue to write to the address\n"
		"-D\t\tPCI debugging\n"
		"-a\t\tRegister address\n"
		"Device:\n"
		"-d [<vendor>]:[<device>]\t\t\tShow selected devices\n"
		"-s [[[[<domain>]:]<bus>]:][<slot>][.[<func>]]"
			"\tShow devices in selected slots\n\n",
		progname, idfile);
}


static int print_register(struct pci_dev *dev, u32 address)
{
	volatile void *mem;
	int dev_mem_fd;

	dev_mem_fd = open("/dev/mem", O_RDONLY);
	if (dev_mem_fd < 0) {
		perror("open");
		return -1;
	}

	mem = mmap(NULL, MEM_WINDOW_SZ, PROT_READ, MAP_SHARED, dev_mem_fd,
			(dev->base_addr[0] & PCI_ADDR_MEM_MASK));
	if (mem == MAP_FAILED) {
		perror("mmap/readable - try rebooting with iomem=relaxed");
		close(dev_mem_fd);
		return -1;
	}

	printf("0x%x\n", *((u32 *)(mem + address)));

	munmap((void *)mem, MEM_WINDOW_SZ);
	close(dev_mem_fd);

	return 0;
}


static int write_register(struct pci_dev *dev, u32 address, u32 value)
{
	volatile void *mem;
	int dev_mem_fd;

	dev_mem_fd = open("/dev/mem", O_RDWR);
	if (dev_mem_fd < 0) {
		perror("open");
		return -1;
	}

	mem = mmap(NULL, MEM_WINDOW_SZ, PROT_WRITE, MAP_SHARED, dev_mem_fd,
			(dev->base_addr[0] & PCI_ADDR_MEM_MASK));
	if (mem == MAP_FAILED) {
		perror("mmap/writable - try rebooting with iomem=relaxed");
		close(dev_mem_fd);
		return -1;
	}

	*((u32 *)(mem + address)) = value;
	printf("0x%x\n", *((u32 *)(mem + address)));

	close(dev_mem_fd);
	munmap((void *)mem, MEM_WINDOW_SZ);

	return 0;
}


int main(int argc, char **argv)
{
	int ch;
	struct pci_access *pacc;
	struct pci_dev *dev;
	char *errmsg;
	char buf[128];
	u32 address = 0;
	u32 value = 0;
	u64 lvalue = 0;
	int device_specified = 0;
	int do_write = 0;
	int got_address = 0;
	int ret;

	if (getuid() != 0) {
		printf("%s: must be run as root\n", argv[0]);
		exit(1);
	}

	pacc = pci_alloc();		/* Get the pci_access structure */
	if (pacc == NULL) {
		perror("pci_alloc");
		exit(1);
	}
	pci_filter_init(pacc, &filter);

	while ((ch = getopt_long(argc, argv, "w:a:d:s:", opts, NULL)) != -1) {
		switch (ch) {
		case 'w':
			lvalue = strtoll(optarg, NULL, 0);
			value = (u32)lvalue;
			do_write++;
			break;
		case 'D':
			pacc->debugging++;
			break;
		case 'a':
			address = strtol(optarg, NULL, 0);
			got_address++;
			break;
		case 'd':
			/* Show only selected devices */
			if ((errmsg = pci_filter_parse_id(&filter, optarg))) {
				printf("%s\n", errmsg);
				exit(1);
			}
			if ((filter.vendor >= 0) &&
				(filter.vendor != PCI_VENDOR_ID_INTEL)) {
				printf("Only Intel devices are supported!\n");
				exit(1);
			}
			device_specified++;
			break;
		case 's':
			/* Show only devices in selected slots */
			if ((errmsg = pci_filter_parse_slot(&filter, optarg))) {
				printf("%s\n", errmsg);
				exit(1);
			}
			device_specified++;
			break;
		case '?':
		default:
			usage(argv[0], pacc->id_file_name);
			exit(1);
			break;
		}
	}

	if (!device_specified) {
		printf("No device given\n");
		usage(argv[0], pacc->id_file_name);
		exit(1);
	}

	if (!got_address) {
		printf("No address given\n");
		usage(argv[0], pacc->id_file_name);
		exit(1);
	}

	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* Get the list of devices */

	filter.vendor = PCI_VENDOR_ID_INTEL;	/* only interested in Intel stuff */

	if (pacc->debugging)
		printf(	"filter: "
			"bus=0x%x slot=0x%x func=0x%x\n"
			"\tvendor=0x%x device=0x%x\n\n",
			filter.bus, filter.slot, filter.func,
			filter.vendor, filter.device);

	/* Iterate over all devices to find the single one we want */
	for (dev = pacc->devices; dev; dev = dev->next) {
		if (pci_filter_match(&filter, dev))
			break;
	}

	if (!dev) {
		printf("no device found\n");
		ret = -1;
		goto out;
	}

	/* Fill in header info we need */
	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_SIZES);

	printf(	"%02x:%02x.%d (%04x:%04x)\n%s\n",
			dev->bus, dev->dev, dev->func, 
			dev->vendor_id, dev->device_id,
			pci_lookup_name(pacc, buf, sizeof(buf),
				PCI_LOOKUP_VENDOR|PCI_LOOKUP_DEVICE, 
				dev->vendor_id, dev->device_id, 0, 0));

	if (do_write)
		ret = write_register(dev, address, value);
	else
		ret = print_register(dev, address);

out:
	pci_cleanup(pacc);		/* Close everything */
	return ret;
}

