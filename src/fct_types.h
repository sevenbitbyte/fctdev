#ifndef FCT_TYPES_H
#define FCT_TYPES_H

#include <vector>
#include <inttypes.h>

struct socket_stats{
	uint64_t txCount;
	uint64_t rxCount;
	uint64_t txFailCount;
	uint64_t rxFailCount;
	uint64_t txBytes;
	uint64_t rxBytes;
};


enum LinkType {
	UNCONFIGURED = 0x00,
	INVALID = 0x00,
	UPLINK = 0x01,
	TRUNK = 0x02,
	DOWNLINK = 0x04,
	INTERNAL = 0x08,
	LOOPBACK = 0x10,
	IMPROPER = 0x20
};


typedef std::vector<uint8_t> vid_t;

enum MMT_NodeTypes{
	MMT_Relay = 0x00,	//!	Used when a node does not have any VIDs
	MMT_Up   = 0x01,
	MMT_Trunk = 0x02,
	MMT_Down = 0x04,
	MMT_ClusterHead = 0x80
};

#endif //FCT_TYPES_H
