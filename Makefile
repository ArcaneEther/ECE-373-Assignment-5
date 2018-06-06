all:
	gcc pci_userspace.c -o pci_userspace -lpci -lz
	
clean:
	rm pci_userspace