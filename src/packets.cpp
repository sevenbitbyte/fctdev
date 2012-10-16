#include "debug.h"
#include "fct_types.h"
#include "rawutils.h"
#include "packets.h"
#include "emulabutils.h"
#include <math.h>

#include <sstream>

#define PACKET_PARSE_DEBUG	false

string linkTypeToString(uint8_t type){
	std::ostringstream sstr;

	if((type&IMPROPER) != 0){
		sstr<<"IMPROPER,";
	}

	if((type&UPLINK) != 0){
		sstr<<"UPLINK,";
	}

	if((type&TRUNK) != 0){
		sstr<<"TRUNK,";
	}

	if((type&DOWNLINK) != 0){
		sstr<<"DOWNLINK,";
	}

	if((type&INTERNAL) != 0){
		sstr<<"INTERNAL,";
	}

	return sstr.str();
}

string mmtTypeToString(uint8_t type){
	std::ostringstream sstr;

	/*if(type == MMT_Unconfigured){
		sstr<<"Unconfigured";
	}*/

	if(type ==MMT_Relay){
		sstr<<"Relay,";
	}

	if((type&MMT_Up) != 0){
		sstr<<"UP,";
	}

	if((type&MMT_Trunk) != 0){
		sstr<<"TRUNK,";
	}

	if((type&MMT_Down) != 0){
		sstr<<"DOWN,";
	}

	if((type&MMT_ClusterHead) != 0){
		sstr<<"ClusterHead";
	}

	return sstr.str();
}

string vidToString(vid_t vid){
	std::ostringstream sstr;

	sstr.unsetf(ios::dec);
	sstr.setf(ios::hex);

	for(uint i=0; i<vid.size(); i++){
		sstr<<vid[i];

		if(i < vid.size()-1){
			sstr<<".";
		}
	}

	return sstr.str();
}

string vidListToString(vector<vid_t> vidList){
	std::ostringstream sstr;

	sstr<<"{";

	for(uint i=0; i<vidList.size(); i++){
		sstr << vidToString(vidList[i]);

		if(i < vidList.size()-1){
			sstr << ", ";
		}
	}

	sstr<<"}";

	return sstr.str();
}

string packetTypeToString(raw_packet* packet){
	std::ostringstream sstr;

	uint16_t proto = packet->readPacketType();

	if(proto == PROTO_SI_CMP){
		sstr<<"SI_CMP" << cmpTypeToString(packet->payloadPtr());
	}
	else if(proto == PROTO_MMT){
		uint8_t action = packet->payloadPtr()[packet->readPayloadLength()];

		sstr<<"MMT" << mmtActionToString(action);
	}
	else if(proto == PROTO_SI_DATA){
		sstr<<"SI_DATA";
	}
	else{
		sstr.unsetf(ios::dec);
		sstr.setf(ios::hex);
		sstr<<"UNKNOWN,"<<proto;
	}

	return sstr.str();
}

string mmtActionToString(uint8_t val){
	std::ostringstream sstr;

	switch(val){
		case MMT_Advertise:
			sstr<<".Advertise";
			break;
		case MMT_JoinRequest:
			sstr<<".JoinRequest";
			break;
		case MMT_JoinRequestCH:
			sstr<<".JoinRequestCH";
			break;
		case MMT_JoinAccept:
			sstr<<".JoinAccept";
			break;
		case MMT_Inform:
			sstr<<".Inform";
			break;
		case MMT_Notify:
			sstr<<".Notify";
			break;
		case MMT_Forward:
			sstr<<".Forward";
			break;
		default:
			break;
	}

	return sstr.str();
}

string cmpTypeToString(uint8_t* payloadPtr){
	std::ostringstream sstr;

	switch(CMP_MAJOR_TYPE(payloadPtr)){
		case CMP_CloudAd:
			sstr<<".CloudAd";
			switch(CMP_MINOR_TYPE(payloadPtr)){
				case ADV_Alive:
					sstr<<".Alive";
					break;
				case ADV_ForcedAdv:
					sstr<<".ForcedAdv";
					break;
				case ADV_ListsSet:
					sstr<<".ListsSet";
					break;
				case ADV_ListsRemove:
					sstr<<".ListsRemove";
					break;
				default:
					break;
			}

			break;
		case CMP_Config:
			sstr<<".Config";
			switch(CMP_MINOR_TYPE(payloadPtr)){
				case CONF_SetSIAddr:
					sstr<<"SetSIAddr";
					break;
				case CONF_SetMMTUID:
					sstr<<"SetMMTUID";
					break;
				case CONF_SIAddrRequest:
					sstr<<"SIAddrRequest";
					break;
				case CONF_SIAddrOffer:
					sstr<<"SIAddrOffer";
					break;
				case CONF_SIAddrRevoke:
					sstr<<"CONF_SIAddrRevoke";
					break;
				case CONF_MMTUIDRequest:
					sstr<<"MMTUIDRequest";
					break;
				case CONF_MMTUIDOffer:
					sstr<<"MMTUIDOffer";
					break;
				default:
					break;
			}
			break;
		case CMP_DestUnreachable:
			sstr<<".DestUnreachable";
			break;
		case CMP_EchoReply:
			sstr<<".EchoReply";
			break;
		case CMP_EchoRequest:
			sstr<<".EchoRequest";
			break;
		case CMP_Trace:
			sstr<<".Trace";
			break;
		default:
			break;
	}

	return sstr.str();
}


string packetToString(raw_packet* packet){
	std::ostringstream sstr;

	uint16_t payloadLen = packet->readPayloadLength();

	sstr<<"[";
	sstr<<"Type="<<packetTypeToString(packet)<<"; ";
	sstr<<"Dest="<<raw_address::toString(packet->destinationAddrPtr())<<"; ";
	sstr<<"Src="<<raw_address::toString(packet->senderAddrPtr())<<"; ";
	sstr<<"Length="<<byteCountToString(packet->length)<<"; ";
	sstr<<"PayloadLen="<<byteCountToString(payloadLen)<<"; ";

	uint16_t proto = packet->readPacketType();

	uint8_t* ptr = packet->payloadPtr();

	if(proto == PROTO_SI_CMP){
		uint16_t seq = CMP_SEQ_NUM(ptr);
		uint8_t major_type = CMP_MAJOR_TYPE(ptr);
		uint8_t minor_type = CMP_MINOR_TYPE(ptr);
		uint8_t flags = CMP_GET_FLAGS(ptr);

		sstr<<"Seq="<<seq<<"; ";
		sstr<<"Flags="<<(int)flags<<"; ";


		if(major_type == CMP_CloudAd){
			//Read address list
			vector<raw_address> addrs = parseAddressList(ptr+CMP_HDR_LEN);

			sstr<<"AddrList={";

			for(uint32_t i=0; i<addrs.size(); i++){
				raw_address addr = addrs.at(i);
				sstr<<" "<<addr.toString();
			}

			sstr<<"}; ";

			addrs = parseAddressList(skipFields(ptr+CMP_HDR_LEN, 1));

			sstr<<"Trunk={";

			for(uint32_t i=0; i<addrs.size(); i++){
				raw_address addr = addrs.at(i);
				sstr<<" "<<addr.toString();
			}

			sstr<<"}; ";

			sstr<<"Hostname="<<parseString(skipFields(ptr+CMP_HDR_LEN, 2))<<"; ";
		}
		else if(major_type == CMP_Config){
			if(minor_type == CONF_SetSIAddr && payloadLen > CMP_HDR_LEN){
				//Parse address
				sstr<<"Address=" << raw_address::toString(&ptr[CMP_HDR_LEN]) <<"; ";
			}
			else if(minor_type == CONF_SIAddrRequest && payloadLen > CMP_HDR_LEN){
				//Parse address
				sstr<<"Address=" << raw_address::toString(&ptr[CMP_HDR_LEN]) <<"; ";
			}
			else if(minor_type == CONF_SIAddrOffer && payloadLen > CMP_HDR_LEN){
				//Parse address
				sstr<<"Address=" << raw_address::toString(&ptr[CMP_HDR_LEN]) <<"; ";
			}
			else if(minor_type == CONF_SIAddrRevoke && payloadLen > CMP_HDR_LEN){
				//Parse address
				sstr<<"Address=" << raw_address::toString(&ptr[CMP_HDR_LEN]) <<"; ";
			}
		}
	}
	else if(proto == PROTO_MMT){
		uint8_t action = ptr[payloadLen - 1];
		uint16_t uid = ntohs(*((uint16_t*) &ptr[payloadLen - 3]));
		uint16_t length = ntohs( *((uint16_t*) &ptr[payloadLen - MMT_HDR_LEN]));

		uint8_t* mmtPayloadPtr = &ptr[payloadLen - (MMT_HDR_LEN + length)];

		sstr<<"uid="<<uid<<"; ";
		sstr<<"mmtDataLen="<<length<<"; ";

		if(action == MMT_Advertise){
			vector<vid_t> vidList = parseVidList(&mmtPayloadPtr[1]);

			sstr << "mmt_type=" << mmtTypeToString(mmtPayloadPtr[0]) << "; ";
			sstr<< "vid_list=" << vidListToString(vidList) << "; ";
		}
	}

	sstr<<"]";

	return sstr.str();
}


bool buildEchoReply(raw_packet* reply, uint8_t* dest, uint8_t* src, uint16_t seq){
	UNSUPPORTED_MSG(endl);
	return false;
}


bool buildEchoReply(raw_packet* reply, raw_packet* request){
	UNSUPPORTED_MSG(endl);
	return false;
}


/**
  *Payload layout of an annoucement packet
  *[CMD_TYPE][CMD_SEQ][ADDR_BYTES]<ADDR_LIST>[TRUNK_BYTES]<TRUNK_LIST>[NAME_BYTES]<HOSTNAME>
  */
bool buildADVPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, int8_t action, list<raw_address*> addresses, vector<raw_address*> trunkList, uint16_t seq, bool nameEnable){

	if(action==-1){
		if(!addresses.empty() || !trunkList.empty()){
			action=ADV_ListsSet;
		}
		else{
			action=ADV_Alive;
		}
	}

	buildCMPPacket(packet, dest, src, MK_CMP_TYPE(CMP_CloudAd,action), seq);

	bool success=true;
	uint16_t addrBytes = 0;
	list<raw_address*>::iterator addrListIter;

	//Determine address list byte size
	for(addrListIter=addresses.begin(); addrListIter!=addresses.end(); addrListIter++){
		addrBytes += (*addrListIter)->bytes;
	}

	//Write address list byte size
	addrBytes = htons(addrBytes);	//Convert to network byte order
	success &= packet->appendData((uint8_t*)&addrBytes, sizeof(uint16_t));

	//Write addresses
	for(addrListIter=addresses.begin(); addrListIter!=addresses.end(); addrListIter++){
		uint16_t bytes = (*addrListIter)->bytes;

		success &= packet->appendData((*addrListIter)->buffer, bytes);
	}


	addrBytes = 0;
	vector<raw_address*>::iterator rawAddrIter;
	//Determine trunk address list byte size
	for(rawAddrIter=trunkList.begin(); rawAddrIter!=trunkList.end(); rawAddrIter++){
		addrBytes +=  (*rawAddrIter)->bytes;
	}

	//Write trunk address list byte size
	addrBytes = htons(addrBytes);	//Convert to network byte order
	success &= packet->appendData((uint8_t*)&addrBytes, sizeof(uint16_t));

	//Write trunk addresses
	for(rawAddrIter=trunkList.begin(); rawAddrIter!=trunkList.end(); rawAddrIter++){
		uint16_t bytes = (*rawAddrIter)->bytes;

		success &= packet->appendData((*rawAddrIter)->buffer, bytes);
	}

	if(nameEnable){
		//Include hostname for debugging
		string hostName = getNodeName();
		uint16_t netByteOrderLen = hostName.size() + 1;

		netByteOrderLen = htons(netByteOrderLen);	//Convert to network byte order


		//Write hostname to buffer
		success &= packet->appendData((uint8_t*)&netByteOrderLen, sizeof(uint16_t));
		success &= packet->appendData((uint8_t*)hostName.c_str(), hostName.size() + 1);

	}
	else{
		//Write zero as lenth of hostname field
		uint16_t zero=0;
		success &= packet->appendData((uint8_t*)&zero, sizeof(uint16_t));
	}

	#if PACKET_PARSE_DEBUG
	DEBUG_MSG(endl);
	packet->printPacket();
	APPEND_MSG(endl);
	#endif

	return success;
}

/**
  *	Configuration packet layout
  *
  *	[ETH_HDR][FCT_TYPE][PAYLOAD_LENGTH][DEST_ADDR][SRC_ADDR][CMD_TYPE][CMD_SEQ][FLAGS]
  */


/**
  *	General Command Packet layout
  *	[ETH_HEADER][FCT_TYPE][PAYLOAD_LENGTH][DEST_ADDR][SOURCE_ADDR][CMP_TYPE][CMP_SEQ][CMP_FLAGS][CMP_DATA]
  */
uint8_t* buildCMPPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, uint8_t type, uint16_t seq, uint8_t flags){
	uint8_t* dataPtr = packet->init(dest, src, CMP_HDR_LEN);

	packet->setPacketType(PROTO_SI_CMP);

	#if PACKET_PARSE_DEBUG
	DEBUG_MSG(endl);
	packet->printPacket();
	APPEND_MSG(endl);
	#endif

	//Set CMP type
	dataPtr[0] = type;

	//Set sequence number
	/*dataPtr[1]=((uint8_t*)&seq)[0];
	dataPtr[2]=((uint8_t*)&seq)[1];*/

	(*((uint16_t*)&dataPtr[1])) = htons(seq);

	dataPtr[3]=flags;

	#if PACKET_PARSE_DEBUG
	DEBUG_MSG(endl);
	packet->printPacket();
	APPEND_MSG(endl);
	#endif

	return dataPtr;
}

uint8_t* buildMMTPacket(raw_packet* packet, uint8_t* dest, uint8_t* src, uint8_t mmtAction, uint16_t uid, uint16_t payloadLen, uint8_t* payload){
	uint8_t* dataPtr = packet->init(dest, src, MMT_HDR_LEN + payloadLen);

	packet->setPacketType(PROTO_MMT);

	if(payload != NULL){
		//Copy payload into packet
		memcpy(dataPtr, payload, payloadLen);
	}

	//Write payload length
	(*((uint16_t*) &dataPtr[payloadLen])) = htons(payloadLen);

	//Write sender uid
	(*((uint16_t*) &dataPtr[payloadLen+sizeof(uint16_t)])) = htons(uid);

	//Write MMT packet action
	dataPtr[payloadLen+2*sizeof(uint16_t)] = mmtAction;

	return dataPtr;
}

vector<raw_address> parseAddressList(uint8_t* addrListPtr){
	vector<raw_address> addresses;

	uint16_t length = ADDR_LIST_BYTES(addrListPtr);
	if(length > 0){

		uint16_t read = 0;

		while(read < length){
			addresses.push_back(raw_address(&addrListPtr[ADDR_LIST_HDR_LEN+read]));
			read += addresses.back().bytes;

			if(read > length){
				ERROR("Buffer overrun! Read "<<read<<" bytes, expected "<<length<<" bytes"<<endl);

				#if PACKET_PARSE_DEBUG
				ABORT_NOW();
				#endif
			}
		}

		if(read != length){
			ERROR("Address size mismatch! Read "<<read<<" bytes, expected "<<length<<" bytes"<<endl);
			#if PACKET_PARSE_DEBUG
			ABORT_NOW();
			#endif
		}
	}

	return addresses;
}


uint8_t* writeVid(const vid_t& vid, uint8_t* buffer){

	//Write number of bytes that follow
	buffer[0] = (uint8_t) vid.size();

	for(uint i=0; i<vid.size(); i++){
		buffer[i+1] = vid[i];
	}

	return &buffer[1 + vid.size()];
}

uint8_t* writeVidList(const vector<vid_t>& vidList, uint8_t* buffer){
	uint8_t* ptr = &buffer[1];

	//Write number of vids to follow
	buffer[0] = (uint8_t) vidList.size();

	//Iterate over all vids
	for(uint i=0; i<vidList.size(); i++){
		ptr = writeVid(vidList[i], ptr);
	}

	return ptr;
}

uint32_t getVidByteCount(const vid_t& v){
	return 1 + v.size();
}

uint32_t getVidListByteCount(const vector<vid_t>& vidList){
	uint32_t bytes=1;

	for(uint i=0; i<vidList.size(); i++){
		bytes += getVidByteCount(vidList[i]);
	}

	return bytes;
}

vid_t parseVid(uint8_t* vidPtr){
	vid_t vid;
	uint8_t len = vidPtr[0];

	for(uint8_t i=0; i<len; i++){
		vid.push_back(vidPtr[i+1]);
	}

	return vid;
}

vector<vid_t> parseVidList(uint8_t* listPtr){
	vector<vid_t> vidList;
	uint32_t offset = 1;
	uint8_t vidCount = listPtr[0];

	for(uint i=0; i<vidCount; i++){
		vid_t v = parseVid(&listPtr[offset]);

		offset += v.size() + 1;

		vidList.push_back(v);
	}

	return vidList;
}


string parseString(uint8_t* stringPtr){
	string retStr;

	uint16_t length = ADDR_LIST_BYTES(stringPtr);

	if(length > 0){
		retStr.append((char*)(stringPtr+sizeof(uint16_t)), length);
	}

	return retStr;
}


uint8_t* skipFields(uint8_t* payloadPtr, int count){
	#if PACKET_PARSE_DEBUG
	uint64_t ptr = (uint64_t) &(*payloadPtr);
	DEBUG_MSG("pointer = "<<ptr<<endl);
	#endif

	for(int i=0; i<count; i++){
		uint16_t length = ADDR_LIST_BYTES(payloadPtr);

		#if PACKET_PARSE_DEBUG
		DEBUG_MSG("At i="<<i<<" skipping "<<length<<"bytes"<<endl);
		#endif

		payloadPtr += length + sizeof(uint16_t);
	}

	return payloadPtr;
}


