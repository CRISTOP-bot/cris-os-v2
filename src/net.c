#include "net.h"
#include "kstring.h"
#include "console.h"
#include "memory.h"
#include "asm.h"

static struct net_interface interfaces[NET_MAX_INTERFACES];
static int interface_count = 0;

static struct net_packet rx_queue[NET_MAX_PACKETS];
static int rx_head = 0;
static int rx_tail = 0;

void net_init(void)
{
	kmemset(interfaces, 0, sizeof(interfaces));
	interface_count = 0;

	/* Create loopback interface */
	interfaces[0].up = true;
	kstrcpy(interfaces[0].name, "lo", 8);
	interfaces[0].mac[0] = 0x00; interfaces[0].mac[1] = 0x00;
	interfaces[0].mac[2] = 0x00; interfaces[0].mac[3] = 0x00;
	interfaces[0].mac[4] = 0x00; interfaces[0].mac[5] = 0x01;
	interfaces[0].ip[0] = 127; interfaces[0].ip[1] = 0;
	interfaces[0].ip[2] = 0;   interfaces[0].ip[3] = 1;
	interfaces[0].subnet[0] = 255; interfaces[0].subnet[1] = 0;
	interfaces[0].subnet[2] = 0;   interfaces[0].subnet[3] = 0;
	interface_count = 1;

	console_print("[ OK ] Network initialized (loopback)\n");
}

int net_send_packet(int iface, const uint8_t *data, uint32_t size)
{
	if (iface < 0 || iface >= interface_count)
		return -1;
	if (!interfaces[iface].up)
		return -1;
	if (size > NET_MAX_PACKET_SIZE)
		return -1;

	interfaces[iface].tx_packets++;
	interfaces[iface].tx_bytes += size;

	/* Loopback: echo packet back */
	if (iface == 0) {
		if ((rx_head + 1) % NET_MAX_PACKETS == rx_tail)
			return -1;
		kmemcpy(rx_queue[rx_head].data, data, size);
		rx_queue[rx_head].size = size;
		rx_queue[rx_head].interface = iface;
		rx_head = (rx_head + 1) % NET_MAX_PACKETS;
		return (int)size;
	}

	return (int)size;
}

int net_recv_packet(int iface, uint8_t *buf, uint32_t max_size)
{
	(void)iface;
	if (rx_head == rx_tail)
		return -1;

	if (rx_queue[rx_tail].size > max_size)
		return -1;

	kmemcpy(buf, rx_queue[rx_tail].data, rx_queue[rx_tail].size);
	int size = (int)rx_queue[rx_tail].size;
	rx_tail = (rx_tail + 1) % NET_MAX_PACKETS;
	return size;
}

void net_handle_arp(const uint8_t *data, uint32_t size, int iface)
{
	(void)data; (void)size; (void)iface;
}

void net_handle_ip(const uint8_t *data, uint32_t size, int iface)
{
	(void)data; (void)size; (void)iface;
}

int net_get_interface_count(void)
{
	return interface_count;
}

const struct net_interface *net_get_interface(int index)
{
	if (index < 0 || index >= interface_count)
		return 0;
	return &interfaces[index];
}

void net_set_ip(int iface, uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
	if (iface < 0 || iface >= interface_count)
		return;
	interfaces[iface].ip[0] = ip0;
	interfaces[iface].ip[1] = ip1;
	interfaces[iface].ip[2] = ip2;
	interfaces[iface].ip[3] = ip3;
}

void net_set_mac(int iface, uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5)
{
	if (iface < 0 || iface >= interface_count)
		return;
	interfaces[iface].mac[0] = mac0;
	interfaces[iface].mac[1] = mac1;
	interfaces[iface].mac[2] = mac2;
	interfaces[iface].mac[3] = mac3;
	interfaces[iface].mac[4] = mac4;
	interfaces[iface].mac[5] = mac5;
}
