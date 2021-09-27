#include <linux/skbuff.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/udp.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/in.h>


int PPSYNC_SPLIT = 0;
EXPORT_SYMBOL(PPSYNC_SPLIT);

u16 N_PPSYNC_PORTS = 0;
EXPORT_SYMBOL(N_PPSYNC_PORTS);

u16 PPSYNC_PORTS[10] = {0};
EXPORT_SYMBOL(PPSYNC_PORTS);


static int __port_is_high_priority(u16 port)
{
	int i;

	if (N_PPSYNC_PORTS == 0)
		return 0;
	for (i = 0; i < N_PPSYNC_PORTS; i++) {
		if (port == PPSYNC_PORTS[i])
			return 1;
	}
	return 0;
}

static int __skb_check_l3_l4(u8 *cursor)
{
	struct iphdr *iph;
	struct udphdr *udph;
	struct tcphdr *tcph;
	u16 VXLAN_PORT = 4789;

	iph = (struct iphdr *)cursor;
	cursor += sizeof(*iph);
	if (iph->protocol == IPPROTO_UDP) {
		udph = (struct udphdr *)cursor;
		if (ntohs(udph->dest) == VXLAN_PORT) {
			cursor += (sizeof(*udph) + 8 + 14);
			return __skb_check_l3_l4(cursor);
		} else if (__port_is_high_priority(ntohs(udph->dest)) ||
			   __port_is_high_priority(ntohs(udph->source))) {
			return 1;
		} else {
			return 0;
		}
	} else if (iph->protocol == IPPROTO_TCP) {
		tcph = (struct tcphdr *)cursor;
		if (__port_is_high_priority(ntohs(tcph->dest)) ||
		    __port_is_high_priority(ntohs(tcph->source)))
			return 1;
		else
			return 0;
	}

	return 0;
}

int skb_is_high_priority(const struct sk_buff *skb)
{
	u8 *cursor = skb->head;
	u16 skb_mac_h_offset = skb->mac_header;
	u16 skb_ip_h_offset = skb_mac_h_offset + 14;
	cursor += skb_ip_h_offset;
	return __skb_check_l3_l4(cursor);
}
EXPORT_SYMBOL(skb_is_high_priority);