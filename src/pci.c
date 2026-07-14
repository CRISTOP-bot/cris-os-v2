#include "pci.h"
#include "console.h"
#include "kstring.h"
#include "asm.h"
#include <stdint.h>

static struct pci_device devices[PCI_MAX_DEVICES];
static int device_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	uint32_t addr = (uint32_t)((bus << 16) | (device << 11) | (function << 8) |
				   (offset & 0xFC) | 0x80000000);
	outl(PCI_CONFIG_ADDR, addr);
	return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
	uint32_t addr = (uint32_t)((bus << 16) | (device << 11) | (function << 8) |
				   (offset & 0xFC) | 0x80000000);
	outl(PCI_CONFIG_ADDR, addr);
	outl(PCI_CONFIG_DATA, value);
}

static bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function)
{
	uint32_t vendor = pci_config_read(bus, device, function, 0x00);
	return (vendor & 0xFFFF) != 0xFFFF;
}

static void pci_scan_bus(uint8_t bus);

static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t function)
{
	if (device_count >= PCI_MAX_DEVICES)
		return;

	uint32_t class_reg = pci_config_read(bus, device, function, 0x08);
	uint32_t vendor_reg = pci_config_read(bus, device, function, 0x00);

	devices[device_count].vendor_id = vendor_reg & 0xFFFF;
	devices[device_count].device_id = (vendor_reg >> 16) & 0xFFFF;
	devices[device_count].class_code = (class_reg >> 24) & 0xFF;
	devices[device_count].subclass = (class_reg >> 16) & 0xFF;
	devices[device_count].prog_if = (class_reg >> 8) & 0xFF;
	devices[device_count].revision = class_reg & 0xFF;
	devices[device_count].bus = bus;
	devices[device_count].device = device;
	devices[device_count].function = function;
	device_count++;

	/* if this is a PCI-to-PCI bridge, scan the secondary bus */
	if (devices[device_count - 1].class_code == 0x06 &&
	    devices[device_count - 1].subclass == 0x04) {
		uint32_t bus_reg = pci_config_read(bus, device, function, 0x18);
		uint8_t secondary_bus = (bus_reg >> 8) & 0xFF;
		if (secondary_bus)
			pci_scan_bus(secondary_bus);
	}
}

static void pci_scan_device(uint8_t bus, uint8_t device)
{
	if (!pci_device_exists(bus, device, 0))
		return;

	pci_scan_function(bus, device, 0);

	uint32_t header = pci_config_read(bus, device, 0, 0x0C);
	uint8_t header_type = (header >> 16) & 0xFF;
	if (!(header_type & 0x80)) {
		/* single function device */
		return;
	}
	/* multi-function device */
	for (uint8_t function = 1; function < 8; function++) {
		if (pci_device_exists(bus, device, function))
			pci_scan_function(bus, device, function);
	}
}

static void pci_scan_bus(uint8_t bus)
{
	for (uint8_t device = 0; device < 32; device++)
		pci_scan_device(bus, device);
}

void pci_init(void)
{
	device_count = 0;

	pci_scan_bus(0);

	console_print("[ OK ] PCI: ");
	char buf[16];
	kitoa(device_count, buf, sizeof(buf));
	console_print(buf);
	console_print(" device(s) found\n");
}

const struct pci_device *pci_get_device(int index)
{
	if (index < 0 || index >= device_count)
		return 0;
	return &devices[index];
}

int pci_get_device_count(void)
{
	return device_count;
}
