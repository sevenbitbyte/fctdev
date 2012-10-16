#ifndef PACKETS_H
#define PACKETS_H

#include <arpa/inet.h>
#include "rawutils.h"
#include <stdlib.h>

/** Protocol types **/
//! SI Command Packet
#define PROTO_SI_CMP 1

//! Multi-Meshed Tree Packet
#define PROTO_MMT    2

//! SI Packet containing data
#define PROTO_SI_DATA 5


#ifdef BUILD_RELEASE
	#define INCLUDE_HOSTNAME	false
#else
	#define INCLUDE_HOSTNAME	true
#endif

enum MMTPacketTypes{
	MMT_Advertise = 0x0,
	MMT_JoinRequest = 0x1,
	MMT_JoinRequestCH = 0x2,
	MMT_JoinAccept = 0x3,
	MMT_Inform = 0x4,
	MMT_Notify = 0x5,
	MMT_Forward = 0x6
};

enum CMPTypes{
	CMP_EchoReply=0x0,			//!	Sent in response to an Echo Request
	CMP_DestUnreachable=0x1,	//! Unsupported currently
	CMP_EchoRequest=0x2,		//!	Used to request an Echo Reply
	CMP_CloudAd=0x3,			//!	Cloud advertisments
	CMP_Trace=0x4,				//! Unsupported currently
	CMP_Config=0x5				//! Used to negotiate secondary addresses
};

//! Command packet flags
enum CMPFlags{
	CMP_FLAG_ACK=1,
	CMP_FLAG_NAK=2,
	CMP_FLAG_ERR=4,
	CMP_FLAG_ARQ=8
};

//!	Cloud advertisment subtypes
enum AdvertiseCode{
	ADV_Alive=0x0,		//!	Standard ADV used when there are no changes in nodes state
	ADV_ForcedAdv,		//!	Used by announce tool to create routing table entries that cannot be deleted, USED ONLY FOR TESTING
	ADV_ListsSet,		//!	Used to notify a neighbor about new alternate addresses and trunk links
	ADV_ListsRemove		//! Used to remove alternate addresses and trunk links
};

//!	Configuration packet subtypes
enum ConfigCode{
	CONF_SetSIAddr,			//!	Can be used to force a node to change SI/FCT address
	CONF_SetMMTUID,			//!	Can be used to force a ndoe to change its MMT uid
	CONF_SIAddrRequest,		//!	Used to request a secondary address
	CONF_SIAddrOffer,		//!	Used to offer or deny a seconary address request
	CONF_SIAddrRevoke,		//!	Not used yet, can be used to force a node to switch to a different secondary address
	CONF_MMTUIDRequest,		//!	NOT IMPLEMENTED, Thought it could be handy to make a system for dynamically generating unique MMT uid
	CONF_MMTUIDOffer		//! NOT IMPLEMENTED, Would be used as the reply to a CONF_MMTUIDRequest
};

//! Number of bytes in a default MMT header
#define MMT_HDR_LEN 5

//!	Number of bytes in a command packet header
#define CMP_HDR_LEN 4


#define CMP_MAJOR_MASK (0xf0)
#define CMP_MINOR_MASK (0x0f)

#define MK_CMP_TYPE(major,minor) ((uint8_t)((CMP_MAJOR_MASK&(major<<4))|(minor&CMP_MINOR_MASK)))

/**
  *	@param	payloadPtr Pointer to the start of the packet payload
  *	@return Returns the major type code of a command packet as a uint8_t
  */
#define CMP_MAJOR_TYPE(payloadPtr) ((uint8_t) ((payloadPtr[0]&0xf0)>>4))

/**
  *	@param	payloadPtr Pointer to the start of the packet payload
  *	@return Returns the minor type code of a command packet as a uint8_t
  */
#define CMP_MINOR_TYPE(payloadPtr) ((uint8_t) (payloadPtr[0]&0x0f))

#define CMP_GET_FLAGS(payloadPtr) ((uint8_t) (payloadPtr[3]))

#define CMP_SET_FLAGS(payloadPtr, flags) (payloadPtr[3] = flags)

/**
  *	@param	payloadPtr Pointer to the start of the packet payload
  *	@return	Returns the sequence number of a command packet as a uint16_t
  */
#define CMP_SEQ_NUM(payloadPtr) ntohs(*((uint16_t*)&payloadPtr[1]))

#define CMP_SET_SEQ_NUM(payloadPtr, seqNum) ((*((uint16_t*)&payloadPtr[1])) = htons(seqNum))

#define CMP_SEQ_INCREMENT(payloadPtr) CMP_SET_SEQ_NUM(payloadPtr, (CMP_SEQ_NUM(payloadPtr)+1))

#define CMP_SEQ_DECREMENT(payloadPtr) CMP_SET_SEQ_NUM(payloadPtr, (CMP_SEQ_NUM(payloadPtr)-1))

#define RAND_SEQ	(rand() % 0xffff)

#define ADDR_LIST_HDR_LEN sizeof(uint16_t)

#define ADDR_LIST_BYTES(addrListPtr) ntohs(*((uint16_t*) addrListPtr))


string linkTypeToString(uint8_t type);

string mmtTypeToString(uint8_t type);

string vidToString(vid_t vid);

string vidListToString(vector<vid_t> vidList);

string packetTypeToString(raw_packet* packet);

string mmtActionToString(uint8_t val);

string cmpTypeToString(uint8_t* payloadPtr);

string packetToString(raw_packet* packet);

/**
  *	Writes an advertisement packet into the provided buffer addressed to dest
  *	and sent from src. The source address is assumed to be the currently selected
  *	primary address.
  *
  *	@param	packet		Pointer to the raw_packet to write into
  *	@param	dest		Pointer to the destination address
  *	@param	src			Source address
  *	@param	addresses	List of secondary addresses to include in advertisement
  *	@param	trunkList	List of trunk address to include in advertisement
  *	@param	seq			Sequcne number of packet, defaults to zero
  *	@param	nameEnable	Include optional hostname of sender in packet for debug,
  *						Defaults to true.
  */

bool buildADVPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, int8_t action=-1, list<raw_address*> addresses=list<raw_address*>(0), vector<raw_address*> trunkList=vector<raw_address*>(0), uint16_t seq=0, bool nameEnable=INCLUDE_HOSTNAME);

bool buildEchoReply(raw_packet* reply, uint8_t* dest, uint8_t* src, uint16_t seq);


/**
  *	Constructs an ECHO_REPLY packet in response to the provided
  *	ECHO_REQUEST packet.
  *
  */
bool buildEchoReply(raw_packet* reply, raw_packet* request);


/**
  *	Constructs a command packet of the specified type
  *
  *	@param	packet	Pointer to a raw_packet to write to
  *	@param	dest	Destination address
  *	@param	src		Source address
  *	@param	type	CMP type code
  *	@param	seq		CMP sequence number, defaults to zero
  */
uint8_t* buildCMPPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, uint8_t type, uint16_t seq=0, uint8_t flags=0);


uint8_t* buildMMTPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, uint8_t mmtAction, uint16_t uid, uint16_t payloadLen=0, uint8_t* payload=NULL);

vector<raw_address> parseAddressList(uint8_t* addrListPtr);


/**
  *	Write a vid to a buffer
  *
  *	@param	vid		Vid to serialize
  *	@param	buffer	Buffer to write vid into
  *
  *	@return	Returns a pointer to the end of the serialized vid.
  */
uint8_t* writeVid(const vid_t& vid, uint8_t* buffer);

uint8_t* writeVidList(const vector<vid_t>& vidList, uint8_t* buffer);

/**
  *	Returns the number of bytes that would be used to serialize the provided
  *	vid. The format of serialized vid's is [digit_count]{digits}
  *
  *	@param	v	Vid to count bytes
  *	@return	The number of bytes needed to serialize the provided vid
  */
uint32_t getVidByteCount(const vid_t& v);

uint32_t getVidListByteCount(const vector<vid_t>& vidList);

vid_t parseVid(uint8_t* vidPtr);

vector<vid_t> parseVidList(uint8_t* listPtr);


string parseString(uint8_t* stringPtr);

uint8_t* skipFields(uint8_t* payloadPtr, int count);

#endif //PACKETS_H
