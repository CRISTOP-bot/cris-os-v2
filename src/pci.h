#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

struct pci_device {
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t  class_code;
	uint8_t  subclass;
	uint8_t  prog_if;
	uint8_t  revision;
	uint8_t  bus;
	uint8_t  device;
	uint8_t  function;
};

#define PCI_MAX_DEVICES 32

void pci_init(void);
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
const struct pci_device *pci_get_device(int index);
int pci_get_device_count(void);

#endif
