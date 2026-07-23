#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>

#define NET_MAX_PACKETS 32
#define NET_MAX_PACKET_SIZE 1518
#define NET_MAX_INTERFACES 4
#define NET_IP_ADDR_LEN 4
#define NET_MAC_LEN 6

struct net_packet {
	uint8_t data[NET_MAX_PACKET_SIZE];
	uint32_t size;
	uint32_t interface;
};

struct net_interface {
	char name[8];
	uint8_t mac[NET_MAC_LEN];
	uint8_t ip[NET_IP_ADDR_LEN];
	uint8_t subnet[NET_IP_ADDR_LEN];
	uint8_t gateway[NET_IP_ADDR_LEN];
	bool up;
	uint32_t rx_packets;
	uint32_t tx_packets;
	uint32_t rx_bytes;
	uint32_t tx_bytes;
};

void net_init(void);
int net_send_packet(int iface, const uint8_t *data, uint32_t size);
int net_recv_packet(int iface, uint8_t *buf, uint32_t max_size);
void net_handle_arp(const uint8_t *data, uint32_t size, int iface);
void net_handle_ip(const uint8_t *data, uint32_t size, int iface);
int net_get_interface_count(void);
const struct net_interface *net_get_interface(int index);
void net_set_ip(int iface, uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void net_set_mac(int iface, uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);

#endif
