#include "switched.h"
#include "debug.h"
#include "arraystream.h"
#include <assert.h>
#include <iostream>
#include <list>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h> // for htons
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <cstring>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>


#define ADDRESS_DEBUG false
#define PACKET_DEBUG false
#define SOCKET_DEBUG false


using namespace std;

/*
 * si_address
 */
si_address::si_address() {
	clear();
}
si_address::si_address(std::string addr) {
	parseString(addr);
}

si_address::si_address(const si_address &other) {
	*this = other;
}

/*si_address::si_address(const raw_address &other) {
	bc = other.tierLevel;

	for(int i=0; i<other.chunks.size(); i++){
		chunks.push_back(other.chunks[i].value);
	}

	extra_type = other.extraData.size();

	for(int i=0; i<other.extraData.size(); i++){
		extra.push_back(other.extraData[i]);
	}
}*/

si_address si_address::linkLocal(void) {
	return si_address("3F.1");
}

si_address si_address::broadCast(void) {
	return si_address("3F.FFF");
}

si_address si_address::downBroadCast(void){
	return si_address("3F.FFE");
}

si_address si_address::trunkBroadCast(void){
	return si_address("3F.FFD");
}

si_address si_address::upLinkBroadCast(void){
	return si_address("3F.FFC");
}

void si_address::clear() {
	bc = 0x3f;
	chunks.clear();
	chunks.push_back(0);
	extra_type = 0;
	extra.clear();
}

int si_address::readFrom(uint8_t *data, int len) {
	array_stream_t *as = MakeArrayStream(data, AS_READ, len);
	bc = ArrayStreamGet8(as, 6);
	chunks.clear();
	extra_type = 0;
	extra.clear();
	uint16_t size = 0;
	do {
		size = ArrayStreamGet8(as, 2);
		if (size > 0) {
			chunks.push_back(ArrayStreamGet16(as, size*4));
		}
		/*else{
			ArrayStreamGet16(as, size*4);	//Still must read next four bits
			break;
		}*/
	} while (size > 0);

	size = ArrayStreamGet16(as, 12);
	if (size > 0) {
		extra_type = ArrayStreamGet16(as, 16);
		while (size-- > 0) {
			extra.push_back(ArrayStreamGet8(as, 8));
		}
	}
	int bitsread = ArrayStreamGetBitCount(as);
	if (bitsread != EOF) {
		if (bitsread % 8 != 0)
			bitsread += 8 - bitsread % 8;
		bitsread /= 8;
	}
	ReleaseArrayStream(as);
	return bitsread;
}

int si_address::byteCount() const {
	int bitCount=6;
	if(chunks.size() >= 0){
		uint8_t size = 0;
		deque<uint16_t>::const_iterator c;
		for (c = chunks.begin(); c != chunks.end(); c++) {
			size = 1;
			if(*c>0xf){
				++size;
			}
			if(*c>0xff){
				++size;
			}
			bitCount+=2;
			bitCount+=size*4;
		}
	}
	bitCount+=14;
	if (extra.size() > 0) {
		bitCount+=16;
		bitCount+=extra.size() * 8;
	}

	int byteCount=bitCount/8;
	if ((bitCount%8) != 0){
		byteCount++;
	}
#if ADDRESS_DEBUG
	DEBUG_MSG("Has "<<bitCount<<"bits and "<<byteCount<<"bytes.\n");
#endif
	return byteCount;
}

int si_address::writeTo(uint8_t *data, int len) const {
	uint8_t size = 0;
	array_stream_t *as;
	as = MakeArrayStream(data, AS_WRITE, len);
	ArrayStreamPut8(as, bc, 6);
	if (chunks.size() == 0) {
		size = 0;
		ArrayStreamPut8(as, size, 2);
		/*size = 0;
		ArrayStreamPut8(as, size, 4);*/
	} else {
		deque<uint16_t>::const_iterator c;
		for (c = chunks.begin(); c != chunks.end(); c++) {
			size = 1;
			if(*c>0xf)
				++size;
			if(*c>0xff)
				++size;
			ArrayStreamPut8(as, size, 2);
			ArrayStreamPut16(as, *c, size*4);
		}
	}
	ArrayStreamPut8(as, 0, 2);
	ArrayStreamPut16(as, extra.size(), 12);
	if (extra.size() > 0) {
		ArrayStreamPut16(as, extra_type, 16);
		deque<uint8_t>::const_iterator e;
		for (e = extra.begin(); e != extra.end(); e++) {
			ArrayStreamPut8(as, *e, 8);
		}
	}
	int spaceused = ArrayStreamGetBitCount(as);
#if ADDRESS_DEBUG
	DEBUG_MSG("Spacedused="<<spaceused<<endl);
#endif
	if (spaceused != EOF) {
		if (spaceused % 8 != 0){
			spaceused=spaceused/8 + 1;
		}
		else{
			spaceused /= 8;
		}
	}

	ReleaseArrayStream(as);

#if ADDRESS_DEBUG
	DEBUG_MSG("Wrote "<< toString() << endl);
#endif

	return spaceused;
}

int si_address::parseString(std::string addr) {

#if ADDRESS_DEBUG
	DEBUG_MSG("Parsing "<< addr << endl);
#endif

	string ext = "";

	//TODO: Is this really necessary? Could use it to also check for invalid chars...
	for(unsigned int i = 0; i < addr.length(); ++i){
		addr[i] = tolower(addr[i]);
		if(isxdigit(addr[i]) == 0 && addr[i] != ':' && addr[i] != '.'){
			return -1;
		}
	}

#if USE_DOUBLE_COLON
	unsigned int c = 0;
	for (c = 0; c < addr.length(); c++) {
		if (c+1 < addr.length() && addr[c] == ':' && addr[c+1] == ':') {
			ext = addr.substr(c+2);
			addr.resize(c);
			break;
		} else if (addr[c] == ':' || addr[c] == '.') {
			addr[c] = '\n';
		}
	}
#endif
	clear();
	chunks.clear();
	int num = 0;
	istringstream iaddr(addr);
	iaddr.unsetf(ios::dec);
	iaddr.setf(ios::hex);
	iaddr >> num;
	bc = num & 0x3f;
#if USE_DOUBLE_COLON
	while(iaddr.good()) {
#else
	while(iaddr.good() && chunks.size() < bc) {
#endif
		iaddr >> num;
		chunks.push_back(num & 0xfff);
	}
#if USE_DOUBLE_COLON
	if (ext.length() > 0) {
		iaddr.clear();
		iaddr.str(ext);
		DEBUG_MSG("USE_DOUBLE_COLON"<<endl);
#else
        if (iaddr.good()) { // "There's more data to read, must be extra data
#endif
		char buffer[5] = {0,0,0,0,0};
		// Read type field first (first 2 bytes of extra data)
		if(iaddr.read(buffer, 4)) {
			buffer[5] = '\0';
			extra_type = parseHex(buffer,4);
			if (num == EOF || !iaddr.good()) {
				return(EOF);
			}
		}
		// Read actual extra data
		while(iaddr.read(buffer, 2)) {
			buffer[2] = '\0';
			num = parseHex(buffer, 2);
			if (num == EOF) {
				return(EOF);
			}
			extra.push_back(num);
		}
	}

#if ADDRESS_DEBUG
	DEBUG_MSG("Read "<< toString() << endl);
#endif
	return(0);
}

int si_address::parseHex(char *string, int len) {
		int retVal = 0;
		for(int i = 0; i < len; ++i) {
			retVal <<= 4;
			if (string[i] >= 'a' && string[i] <= 'f')
				string[i] = string[i]-'a'+10;
			else if (string[i] >= '0' && string[i] <= '9')
				string[i] = string[i]-'0';
			else
				string[i] = 0xf;
			retVal |= string[i];
		}
		return retVal;
}

string si_address::toString() const {
	ostringstream retVal;
	retVal << hex;

	retVal << (unsigned int) bc;

	if(chunks.size() == 0){
		retVal << BC_DELIMITER << "0";
	}
	else{
		deque<uint16_t>::const_iterator iter;
		for(iter=chunks.begin(); iter!=chunks.end(); iter++){
			if(iter==chunks.begin()){
				retVal << BC_DELIMITER << (unsigned int) *iter;
			}
			else{
				retVal << CHUNK_DELIMITER << (unsigned int) *iter;
			}
		}
	}


	if (extra.size() > 0) {
		deque<uint8_t>::const_iterator e;
#if USE_DOUBLE_COLON
		retVal << "::";
#else
		retVal << ":";
#endif
		retVal << extra_type;
		for(e = extra.begin(); e != extra.end(); e++) {
			retVal.width(2);
			retVal.fill('0');
			retVal << (unsigned int)*e;
		}
	}
	return retVal.str();
}

int si_address::matchingChunks(si_address &other){
	int matches=0;

	deque<uint16_t>::iterator c;
	deque<uint16_t>::const_iterator oc;
	for(c = chunks.begin(), oc = other.chunks.begin();
			c != chunks.end() || oc != other.chunks.end();
			c++, oc++) {
		if (*c != *oc){
			break;
		}
		matches++;
	}

	return matches;
}

int si_address::hopsTo(si_address &other){
	int matches=matchingChunks(other);
	return (chunks.size() - matches) + (other.chunks.size() - matches);
}

si_address & si_address::operator=(const si_address &other) {
	clear();
	chunks.clear();
	bc = other.bc;
	deque<uint16_t>::const_iterator c;
	for(c = other.chunks.begin(); c != other.chunks.end(); c++) {
		chunks.push_back(*c);
	}
	extra_type = other.extra_type;
	deque<uint8_t>::const_iterator e;
	for(e = other.extra.begin(); e != other.extra.end(); e++) {
		extra.push_back(*e);
	}
	return *this;
}

si_address & si_address::operator=(const std::string &addr) {
	parseString(addr);
	return *this;
}

bool si_address::operator==(const si_address &other) const {
//TODO:  This needs to eventually compare both full addresses *AND* shortened ones
	if (bc != other.bc || chunks.size() != other.chunks.size()){
#if ADDRESS_DEBUG
		DEBUG_MSG("BC value or chunk size does not match"<<endl);
#endif
		return false;
	}

	deque<uint16_t>::const_iterator c;
	deque<uint16_t>::const_iterator oc;
	for(c = chunks.begin(), oc = other.chunks.begin();
			c != chunks.end() || oc != other.chunks.end();
			c++, oc++) {
		if (*c != *oc)
			return false;
	}
	return true;
}

bool si_address::operator!=(const si_address &other) {
	return !(*this == other);
}


/*
 * si_packet
 */
si_packet::si_packet() {
	clear();
	data_ptr = Pointer<uint8_t>((int)ETH_FRAME_LEN);
	data_len=ETH_FRAME_LEN;
	payloadLen=0;
	setSource(si_address::linkLocal());
	setDestination(si_address::linkLocal());
	siHeaderLen=src_addr.byteCount() + dst_addr.byteCount() + SI_PACKET_DATA_LEN_SIZE + SI_PACKET_TYPE_SIZE;
}

si_packet::si_packet(si_address src, si_address dest, const uint8_t* payload, uint16_t len){
	clear();
	data_ptr = Pointer<uint8_t>((int)ETH_FRAME_LEN);
	data_len=ETH_FRAME_LEN;
	setPayload(payload, len);
	setSource(src);
	setDestination(dest);
	siHeaderLen=src_addr.byteCount() + dst_addr.byteCount() + SI_PACKET_DATA_LEN_SIZE + SI_PACKET_TYPE_SIZE;
}

si_packet::si_packet(const si_packet &other) {
	*this = other;
}

si_packet::~si_packet() {
	clear();
}

void si_packet::clear() {
	rx_socket=NULL;
	tx_socket=NULL;
	src_addr=si_address::linkLocal();
	dst_addr=si_address::linkLocal();
	type = 0;
	data_len = 0;
	dstAddrPacked = false;
	siHeaderLen = 0;
	payloadLen = 0;
	data_ptr.detach();
	timer.reset();
}

void si_packet::setData(const uint8_t *data, uint16_t len) {
	data_len = len;
	data_ptr=Pointer<uint8_t>((uint8_t*)data);
}

void si_packet::copyData(const uint8_t *data, uint16_t len) {
	data_ptr = Pointer<uint8_t>(len);
	memcpy(data_ptr.getObject(), data, len);
	data_len = len;
}

uint8_t *si_packet::getData() const {
	return data_ptr.getObject();
}

Pointer<uint8_t> si_packet::getDataPtr() const {
	return data_ptr;
}

int si_packet::getDataLen() const {
	return data_len;
}

bool si_packet::expandPayload(uint16_t len){
	if(len+byteCount() > data_len){
#if PACKET_DEBUG
		ERROR("Prepending will cause buffer overflow, call failed!")<<endl;
#endif
		return false;
	}
	payloadLen += len;
	return true;
}

bool si_packet::setPayload(const uint8_t* payload, uint16_t len){
	payloadLen=0;
	if(payload!=NULL){
		if(len > (data_len - siHeaderLen)){
			perror("si_packet::setPayload()");
			return false;
		}

		payloadLen=len;
		uint8_t* payloadPtr=(data_ptr.getObject()+data_len)-payloadLen;

		if(payload+len > payloadPtr && payload+len < payloadPtr+payloadLen){
			//Buffers overlap
#if PACKET_DEBUG
			WARNING("Payload buffers overlap.")<<endl;
#endif
			memmove(payloadPtr, payload, len);
		}
		else{
			memcpy(payloadPtr, payload, len);
		}
	}
	return true;
}

bool si_packet::prependPayload(const uint8_t* data, uint16_t len){
	if(getPayloadPtr() == NULL){
		return setPayload(data, len);
	}

	if(len+byteCount() > data_len){
#if PACKET_DEBUG
		ERROR("prepending will cause buffer overflow.")<<endl;
#endif
		return false;
	}

	memcpy(getPayloadPtr()-len, data, len);
	payloadLen += len;
	return true;
}

bool si_packet::decapsulate(){
	uint8_t* data = getPayloadPtr();
	if(data==NULL){
		return false;
	}
	int bytesTotal = 0;
	int bytesRead = 0;

	if (bytesTotal+2 > payloadLen) {
		errno = EIO;
		perror("si_packet::decapsulate()");
		return false;
	}
	type = ntohs(*(uint16_t*)(data+bytesTotal));
	bytesTotal += 2;

	if (bytesTotal+2 > payloadLen) {
		errno = EIO;
		perror("si_packet::decapsulate()");
		return false;
	}
	int newPayloadLen = ntohs(*(uint16_t*)(data+bytesTotal));
	bytesTotal += 2;

	bytesRead = dst_addr.readFrom(data+bytesTotal, payloadLen-bytesTotal);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::decapsulate()");
		return false;
	}
	bytesTotal += bytesRead;
	dstAddrPacked = true;

	bytesRead = src_addr.readFrom(data+bytesTotal, payloadLen-bytesTotal);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::decapsulate()");
		return false;
	}
	bytesTotal += bytesRead;


	if(payloadLen-newPayloadLen != siHeaderByteCount()) {
		errno = EIO;
		perror("si_packet::decapsulate()");
#if PACKET_DEBUG
		ERROR("Over run ")<<payloadLen<<","<<bytesTotal<<","<<data_len<<endl;
#endif
		return false;
	}

	payloadLen=newPayloadLen;
	return true;
}

bool si_packet::encapsulateSIHeader(){
	if(!writeHeaders()){
#if PACKET_DEBUG
		ERROR("Failed to write header to internal buffer")<<endl;
#endif
		return false;
	}

	payloadLen+=siHeaderLen;
	return true;
}

bool si_packet::encapsulatePacket(si_packet& other) {
	if(!other.writeHeaders()){
#if PACKET_DEBUG
		ERROR("Failed to write header to internal buffer.")<<endl;
#endif
		return false;
	}

	return setPayload(other.getHeaderPtr(),  other.siHeaderLen+other.payloadLen);
}

int si_packet::byteCount() const {
	return payloadLen + siHeaderLen + ETH_HLEN;
}

int si_packet::freeBytes() const {
	return data_len - byteCount();
}

uint8_t* si_packet::getPayloadPtr() const {
	if(payloadLen == 0){
		return NULL;
	}
	return (data_ptr.getObject()+data_len)-payloadLen;
}

uint16_t si_packet::payloadLength() const {
	return  payloadLen;
}

uint8_t* si_packet::getHeaderPtr() {
	siHeaderByteCount();
	if(siHeaderLen == 0){
		return NULL;
	}
	return (data_ptr.getObject()+data_len)-(payloadLen + siHeaderLen + ETH_HLEN);
}

int si_packet::siHeaderByteCount() {
	siHeaderLen = dst_addr.byteCount() + src_addr.byteCount() + sizeof(uint16_t) + sizeof(uint16_t);
	return siHeaderLen;
}

bool si_packet::writeHeaders(){
	uint8_t* headerPtr = getHeaderPtr();
	int totalHeaderLen = siHeaderLen + ETH_HLEN;

	int total=0;
	int written = 0;

	unsigned char dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	//Ethernet protocol number
	((ethhdr*)headerPtr)->h_proto = htons(ETHER_SI);
	memcpy(&(((ethhdr*)headerPtr)->h_dest), dest_mac, ETH_ALEN);
	total=ETH_HLEN;

	//SI Packet type
	*((uint16_t*)(headerPtr+total)) = htons(type);
	total += 2;

	//SI payload length
	*((uint16_t*)(headerPtr+total)) = htons(payloadLen);
	total += 2;

	//SI Destination
	written = dst_addr.writeTo(headerPtr+total, totalHeaderLen - total);
	if(written != dst_addr.byteCount()){
		ERROR("wrote "<<written<<"bytes of dest addr"<<endl);
	}
	total += written;

	//SI Source
	written = src_addr.writeTo(headerPtr+total, totalHeaderLen - total);
	if(written != src_addr.byteCount()){
		ERROR("wrote "<<written<<"bytes of src addr"<<endl);
	}
	total += written;

	return (total == totalHeaderLen);
}

int si_packet::parseBuffer(){
	uint8_t* data = data_ptr.getObject();
	int bytesTotal = ETH_HLEN;
	int bytesRead = ETH_HLEN;

	if (bytesTotal+2 > data_len) {
		errno = EIO;
		perror("si_packet::parseBuffer(),type");
		return(EOF);
	}
	type = ntohs(*(uint16_t*)(data+bytesTotal));
	bytesTotal += 2;

	if (bytesTotal+2 > data_len) {
		errno = EIO;
		perror("si_packet::parseBuffer(),payload_len");
		return(EOF);
	}
	payloadLen = ntohs(*(uint16_t*)(data+bytesTotal));
	bytesTotal += 2;

	bytesRead = dst_addr.readFrom(data+bytesTotal, data_len-bytesTotal);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::parseBuffer(),dst_addr");
		return(EOF);
	}
	bytesTotal += bytesRead;
	//dstAddrPacked = true;

	bytesRead = src_addr.readFrom(data+bytesTotal, data_len-bytesTotal);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::parseBuffer(),src_addr");
		return(EOF);
	}
	bytesTotal += bytesRead;

	if(payloadLen+bytesTotal > data_len){
		errno = EIO;
		perror("si_packet::parseBuffer()");
		DEBUG_MSG("Over run "<<payloadLen<<","<<bytesTotal<<","<<data_len<<endl);
		return(EOF);
	}

	if(!setPayload(data+bytesTotal, payloadLen)){
		errno = EIO;
		perror("si_packet::parseBuffer(),setPayload()");
		return(EOF);
	}
	bytesTotal += payloadLen;

	siHeaderByteCount();
	return bytesTotal;
}

int si_packet::readFrom(uint8_t *data, uint16_t len) {
	clear();
	data_ptr = Pointer<uint8_t>(new uint8_t[ETH_FRAME_LEN]);
	data_len=ETH_FRAME_LEN;

	int bytesTotal = len;
	int bytesRead = 0;

	if (len < 2) {
		errno = EIO;
		perror("si_packet::readFrom()");
		ERROR("Faield to parse type"<<endl);
		return(EOF);
	}
	type = ntohs(*(uint16_t*)data);
	data += 2;
	len -= 2;

	if (len < 2) {
		errno = EIO;
		perror("si_packet::readFrom()");
		ERROR("Faield to parse length"<<endl);
		return(EOF);
	}
	payloadLen = ntohs(*(uint16_t*)data);
	data += 2;
	len -= 2;

	if (len < payloadLen) {	//Buffer overrun
		errno = EIO;
		perror("si_packet::readFrom()");
		return(EOF);
	}

	bytesRead = dst_addr.readFrom(data, len);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::readFrom()");
		ERROR("Faield to parse destination"<<endl);
		return(EOF);
	}
	data += bytesRead;
	len -= bytesRead;
	dstAddrPacked = true;

	bytesRead = src_addr.readFrom(data, len);
	if (bytesRead == EOF) {
		errno = EIO;
		perror("si_packet::readFrom()");
		ERROR("Faield to parse source"<<endl);
		return(EOF);
	}
	data += bytesRead;
	len -= bytesRead;

	siHeaderByteCount();
	setPayload(data, payloadLen);
	len -= payloadLen;
	return (bytesTotal - len);
}

string si_packet::toString() {
	ostringstream retVal;
	retVal.unsetf( ios::dec );
	retVal.setf( ios::hex );

	retVal << "[";
	retVal << "Data_Ptr = " << (void*)data_ptr.getObject() << "; ";
	retVal << "HeaderPtr = " << (void*)getHeaderPtr() << "; ";
	retVal << "PayloadPtr = " << (void*)getPayloadPtr() << "; ";
	retVal.setf( ios::dec );
	retVal << "SI Header = "<<siHeaderByteCount()<<"; ";
	retVal << "Destination = " << dst_addr.toString();
	retVal << "; ";
	retVal << "Source = " << src_addr.toString();
	retVal << "; ";
	retVal << "Payload = " << payloadLen << " bytes"<<endl;
	retVal<< (char*) getPayloadPtr() ;

	retVal <<endl;
	retVal << "]";
	return retVal.str();
}

void si_packet::updateBC(si_address* sender){
	if(sender==NULL){ sender = &src_addr; }

	if(sender->bc != 0x3f && dst_addr.bc != 0x3f){
		dst_addr.bc=dst_addr.matchingChunks(*sender);
	}
}

void si_packet::updateDest(si_address* sender){
	if(!dstAddrPacked){
		if(sender==NULL){ sender = &src_addr; }

		dst_addr.bc=dst_addr.matchingChunks(*sender);
		siHeaderLen-=dst_addr.byteCount();
		if(dst_addr.bc>0){
			for(uint32_t i=0; i<dst_addr.bc; i++){
				dst_addr.chunks.pop_front();
			}
		}
		//if(dst_addr.chunks.size() == 0){
			//dst_addr.chunks.push_back(0);
		//}
		siHeaderLen+=dst_addr.byteCount();
		dstAddrPacked=true;
	}
	else{
#if PACKET_DEBUG
		DEBUG_MSG("Not updating")<<endl;
#endif
	}
}

void si_packet::restoreDest(si_address *sender){
	if(dstAddrPacked){
		if(sender==NULL){ sender = &src_addr; }

		siHeaderLen-=dst_addr.byteCount();
		deque<uint16_t>::iterator iter=src_addr.chunks.begin();
		for(uint32_t i=1; i+1<dst_addr.bc; i++){
			iter++;
		}

		for(uint32_t i=0; i+1<dst_addr.bc; i++){
			dst_addr.chunks.push_front(*iter);
			iter--;
		}
		dst_addr.bc=dst_addr.chunks.size();
		siHeaderLen+=dst_addr.byteCount();
		dstAddrPacked=false;
	}
}

void si_packet::setDestination(si_address dest){
	siHeaderLen-=dst_addr.byteCount();
	siHeaderLen+=dest.byteCount();
	dst_addr=dest;
	dstAddrPacked=false;
	//updateBC();
	if(dst_addr.bc != 0x3f && src_addr.bc!=0x3f){
		updateDest();
	}
}

void si_packet::setSource(si_address source){
	if(dstAddrPacked){
		restoreDest();
	}
	siHeaderLen-=src_addr.byteCount();
	siHeaderLen+=source.byteCount();
	src_addr=source;
	//updateBC();
	if(src_addr.bc != 0x3f && dst_addr.bc != 0x3f){
		updateDest();
	}
}

 const si_address& si_packet::getDestination() const{
	return dst_addr;
}

const si_address& si_packet::getSource() const{
	return src_addr;
}

si_packet & si_packet::operator=(const si_packet &other) {
	clear();
	src_addr = other.src_addr;
	dst_addr = other.dst_addr;
	type = other.type;
	data_ptr = other.data_ptr;
	data_len = other.data_len;
	payloadLen = other.payloadLen;
	siHeaderLen = other.siHeaderLen;
	return *this;
}


bool si_packet::operator==(const si_packet &other) {
	if (src_addr != other.src_addr || dst_addr != other.dst_addr || type != other.type){
#if PACKET_DEBUG
		DEBUG_MSG("Header fields do not match")<<endl;
#endif
		return false;
	}

	else if (payloadLen != other.payloadLen){
#if PACKET_DEBUG
		DEBUG_MSG("Payload length not equal")<<endl;
#endif
		return false;
	}
	else if (getPayloadPtr() != other.getPayloadPtr()){
		if(memcmp(getPayloadPtr(), other.getPayloadPtr(), payloadLen) != 0){
#if PACKET_DEBUG
			DEBUG_MSG("Payload data not equal")<<endl;
#endif
			return false;
		}
	}
	return true;
}

bool si_packet::operator!=(const si_packet &other) {
	return !(*this == other);
}


/*
 * si_socket
 */
si_socket::si_socket() {
	sock = 0;
	clear();
}

si_socket::si_socket(std::string ifname) {
	sock = 0;
	clear();
	open(ifname);
}

si_socket::si_socket(const si_socket &other) {
	sock = 0;
	clear();
	this->open(other.ifname);
}

void si_socket::clear() {
	clearStats();

	if (isOpen()){
		this->close();
	}

	ifname = "";
	sock = 0;
	pthread_mutex_init(&recvMutex, NULL);
	pthread_mutex_init(&sendMutex, NULL);
	ifindex = -1;
	for(unsigned int i=0; i<sizeof(src_mac); ++i) src_mac[i] = '\0';
}

bool si_socket::isOpen() const {
	return (sock > 0);
}

bool si_socket::isUp() const {
	if(isOpen()){
		struct ifreq ifr;
		strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);


		if(ioctl(sock, SIOCGIFFLAGS, &ifr) == -1){
			perror("SIOCGIFFLAGS");
			ERROR(endl);
			return false;
		}

		if((ifr.ifr_flags&IFF_UP) != 0){
			return true;
		}
	}
	else{
		ERROR("Socket not open!"<<endl);
	}

	return false;
}

bool si_socket::setState(bool state) const {
	if(isOpen()){
		struct ifreq ifr;
		strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);

		if(ioctl(sock, SIOCGIFFLAGS, &ifr) == -1){
			perror("SIOCGIFFLAGS");
			ERROR(endl);
			return false;
		}

		if(state){
			//Bring interface up
			ifr.ifr_flags |= IFF_UP;
		}
		else{
			//Bring interface down
			ifr.ifr_flags &= ~IFF_UP;
		}

		if(ioctl(sock, SIOCSIFFLAGS, &ifr) == -1){
			perror("SIOCSIFFLAGS");
			ERROR(endl);
			return false;
		}
		return true;
	}
	else{
		ERROR("Socket not open!" << endl);
	}

	return false;
}

int si_socket::open(std::string ifname, bool ignoreDown) {
	if (isOpen()){
		clear();
	}
	this->ifname = ifname;
	sock = socket(AF_PACKET, SOCK_RAW, htons(ETHER_SI));
	if (sock < 0) {
		perror("Socket error");
		clear();
		ERROR(endl);
		return -1;
	}
	struct ifreq ifr;
	strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);


	if(ioctl(sock, SIOCGIFFLAGS, &ifr) == -1){
		perror("SIOCGIFFLAGS");
		clear();
		ERROR(endl);
		return -1;
	}

	if(!(ifr.ifr_flags&IFF_UP) && ignoreDown==false){
		ERROR("Interface is not up"<<endl);
		clear();
		return -1;
	}

	if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		clear();
		ERROR(endl);
		return -1;
	}

	if(ifr.ifr_flags&IFF_LOOPBACK){
		WARNING("Interface is LOOPBACK"<<endl);
	}

	ifindex = ifr.ifr_ifindex;
#if SOCKET_DEBUG
	DEBUG_MSG("interface index="<<ifindex<<endl);
#endif
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
		perror("SIOCGIFHWADDR");
		clear();
		ERROR(endl);
		return -1;
	}
	int i = 0;
	for (i = 0; i < 6; ++i)
		src_mac[i] = ifr.ifr_hwaddr.sa_data[i];

	struct sockaddr_ll sa;
	sa.sll_family = PF_PACKET;
	sa.sll_protocol = htons(ETHER_SI);
	sa.sll_ifindex = ifindex;
	sa.sll_hatype = ARPHRD_ETHER;
	sa.sll_pkttype = PACKET_OTHERHOST;
	sa.sll_halen = ETH_ALEN;
	for (i = 0; i < 6; ++i)
		sa.sll_addr[i] = 0x00;
	sa.sll_addr[6] = 0x00;
	sa.sll_addr[7] = 0x00;
	if (bind(sock, (const sockaddr*)&sa, sizeof(sa))) {
		perror("BIND");
		clear();
		ERROR(endl);
		return -1;
	}

	return 0;
}

void si_socket::close() {
	if (isOpen()) {
		::close(sock);
		sock = 0;
	}
	pthread_mutex_destroy(&sendMutex);
	pthread_mutex_destroy(&recvMutex);
}

bool si_socket::canRead(int msTimeout){
	fd_set socket_fds;
	int fds_max=0;

	//Setup list of sockets to select
	FD_ZERO(&socket_fds);
	fds_max=sock+1;
	FD_SET(sock, &socket_fds);
	if(!FD_ISSET(sock, &socket_fds)){
		ERROR("FD_SET failed for interface "<<getName()<<endl);
		return false;
	}

	timeval timeout;
	timeout.tv_sec=msTimeout/1000;
	timeout.tv_usec=(msTimeout-timeout.tv_sec*1000)*1000;



	Timer elapsed(true);
	int retVal=select(fds_max, &socket_fds, NULL, NULL, &timeout);
	elapsed.stop();

	if(retVal < 0){
		if(errno == EINTR){	//Unexpected signal caused select to return prematurely
			timeout=elapsed.getElapsedReal();
			msTimeout-=TIMEVAL_TO_MS((timeout));
			if(msTimeout > 1){
				return canRead(msTimeout);
			}
		}
		else{
			ERROR("si_socket::canRead - ERROR, select returned "<<retVal<<endl);
			perror("select");
		}
	}
	else{
		if(FD_ISSET(sock, &socket_fds)){
			return true;
		}
	}
	return false;
}

int si_socket::send(si_packet &packet) {
	if (!isOpen()) {
		errno = EBADF;
		return -1;
	}

	if(pthread_mutex_trylock(&sendMutex) != 0){
		perror("pthread_mutex_trylock");
		return -2;
	}


	int send_result = 0;

	/*struct sockaddr_ll sa;
	memset((void*)&sa, '\0', sizeof(sa));
	sa.sll_family = PF_PACKET;
	sa.sll_protocol = htons(ETHER_SI);
	sa.sll_ifindex = ifindex;
	sa.sll_hatype = ARPHRD_ETHER;
	sa.sll_pkttype = PACKET_HOST;
	sa.sll_halen = ETH_ALEN;
	for (int i = 0; i < 6; ++i){
		sa.sll_addr[i] = 0xff;
	}
	sa.sll_addr[6] = 0x00;
	sa.sll_addr[7] = 0x00;*/

	packet.tx_socket=this;
	if(!packet.writeHeaders()){
		perror("si_packet::writeHeaders()");
		pthread_mutex_unlock(&sendMutex);
		return -5;
	}
	memcpy(((ethhdr*)packet.getHeaderPtr())->h_source, src_mac, ETH_ALEN);	//Insert source MAC

	packet.tx_socket=this;

#if SOCKET_DEBUG
	DEBUG_MSG("Packet data: ");
	raw_packet temp(packet);
	temp.printPacket();
	cout<<endl;
#endif

	send_result = ::send(sock, packet.getHeaderPtr(), packet.byteCount(), 0); //, (struct sockaddr*)&sa, sizeof(sa));

	if (send_result < 0) {
		string errMsg=this->getName();
		errMsg.append(" si_socket::send(sendto())");
		perror(errMsg.c_str());
		stats.txFailCount++;
		pthread_mutex_unlock(&sendMutex);
		return -3;
	}

	if(send_result != packet.byteCount()){
		stats.txFailCount++;
		pthread_mutex_unlock(&sendMutex);
		return -4;
	}

	/*for (int i = 0; i < packet.byteCount(); i++) {
		printf("%d=%0x ", i, packet.getHeaderPtr()[i]);
		fflush(stdout);
	}*/

	stats.txCount++;
	stats.txBytes += packet.byteCount();
	pthread_mutex_unlock(&sendMutex);
	return send_result;
}

int si_socket::send(raw_packet& packet){
	if (!isOpen()) {
		errno = EBADF;
		return -1;
	}

	if(pthread_mutex_trylock(&sendMutex) != 0){
		perror("pthread_mutex_trylock");
		return -2;
	}

	int send_result = 0;

	/*struct sockaddr_ll sa;
	memset((void*)&sa, '\0', sizeof(sa));
	sa.sll_family = PF_PACKET;
	sa.sll_protocol = htons(ETHER_SI);
	sa.sll_ifindex = ifindex;
	sa.sll_hatype = ARPHRD_ETHER;
	sa.sll_pkttype = PACKET_HOST;
	sa.sll_halen = ETH_ALEN;
	for (int i = 0; i < 6; ++i){
		sa.sll_addr[i] = 0xff;
	}
	sa.sll_addr[6] = 0x00;
	sa.sll_addr[7] = 0x00; */

	//Broadcast mac address
	unsigned char dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	char* buf=(char*)&packet.data[packet.offset];


	//Ethernet protocol number
	((ethhdr*)buf)->h_proto = htons(ETHER_SI);
	memcpy(&(((ethhdr*)buf)->h_dest), dest_mac, ETH_ALEN);
	memcpy(((ethhdr*)buf)->h_source, src_mac, ETH_ALEN);	//Insert source MAC

	send_result = ::send(sock, buf, packet.length, MSG_DONTWAIT); //, (struct sockaddr*)&sa, sizeof(sa));

	if (send_result < 0) {
		string errMsg=this->getName();
		errMsg.append(" si_socket::send(sendto())");
		perror(errMsg.c_str());
		stats.txFailCount++;
		pthread_mutex_unlock(&sendMutex);
		return -3;
	}

	if(send_result != packet.length){
		stats.txFailCount++;
		return -4;
	}

	stats.txCount++;
	stats.txBytes += packet.length;
	pthread_mutex_unlock(&sendMutex);
	return send_result;
}

si_packet* si_socket::recv(bool blocking) {
	if (!isOpen()) {
		errno = EBADF;
		return NULL;
	}
	if(pthread_mutex_trylock(&recvMutex) != 0){
		perror("pthread_mutex_trylock");
		return NULL;
	}
	int flags = 0;
	if (!blocking){
		flags = MSG_DONTWAIT;
	}

	si_packet* packet=new si_packet();
	packet->timer.start();

	int len = ::recv(sock, packet->getData(), ETH_FRAME_LEN, flags);
	if (len < 0) {
		if (errno == EAGAIN) {
			stats.rxFailCount++;
			pthread_mutex_unlock(&recvMutex);
			delete packet;
			return NULL;
		} else {
			stats.rxFailCount++;
			perror("si_socket::recv(recv())");
			pthread_mutex_unlock(&recvMutex);
			delete packet;
			return NULL;
		}
	} else {
		int parseLen=packet->parseBuffer();
		if(parseLen != packet->byteCount()){
			stats.rxFailCount++;
			perror("si_packet::parseBuffer()");
			ERROR("RXLen="<<len<<" != ParsedLen="<<parseLen<<endl);
			pthread_mutex_unlock(&recvMutex);
			delete packet;
			return NULL;
		}
	}
	packet->rx_socket=this;
	stats.rxCount++;
	stats.rxBytes += len;
	pthread_mutex_unlock(&recvMutex);
	return(packet);
}

int si_socket::recv(si_packet &packet, bool blocking) {
	if (!isOpen()) {
		errno = EBADF;
		return -1;
	}
	if(pthread_mutex_trylock(&recvMutex) != 0){
		perror("pthread_mutex_trylock");
		return -2;
	}
	int flags = 0;
	if (!blocking)
		flags = MSG_DONTWAIT;
	unsigned char buf[ETH_FRAME_LEN];
	int len = ::recv(sock, buf, ETH_FRAME_LEN, flags);
	if (len < 0) {
		if (errno == EAGAIN) {
			stats.rxFailCount++;
			pthread_mutex_unlock(&recvMutex);
			return(len);
		}

		stats.rxFailCount++;
		perror("si_socket::recv(recvfrom())");
		pthread_mutex_unlock(&recvMutex);
		return(len);
	}

	packet.readFrom(buf+ETH_HLEN, len-ETH_HLEN);
	packet.rx_socket=this;
	stats.rxCount++;
	stats.rxBytes += len;
	pthread_mutex_unlock(&recvMutex);
	return(0);
}

int si_socket::recv(raw_packet& packet, bool blocking){
	if (!isOpen()) {
		errno = EBADF;
		return -1;
	}

	if(pthread_mutex_trylock(&recvMutex) != 0){
		perror("pthread_mutex_trylock");
		return -2;
	}

	int flags = 0;
	if (!blocking) {
		flags = MSG_DONTWAIT;
	}

	packet.clear();
	packet.rx_socket=this;
	//packet.offset=0;
	packet.length = ::recv(sock, packet.data, ETH_FRAME_LEN, flags);

#if SOCKET_DEBUG
	DEBUG_MSG("Read "<<packet.length<<"bytes with "<<packet.readPayloadLength()<<" payload bytes"<<endl);
#endif

	if (packet.length < 0) {
		if (errno == EAGAIN) {
			//stats.rxFailCount++;
			//perror("si_socket::recv(recvfrom())");
			pthread_mutex_unlock(&recvMutex);
			return(packet.length);
		}

		stats.rxFailCount++;
		perror("si_socket::recv(recvfrom())");
		pthread_mutex_unlock(&recvMutex);
		return(packet.length);
	}

	stats.rxCount++;
	stats.rxBytes += packet.length;
	pthread_mutex_unlock(&recvMutex);
	return(0);
}

socket_stats si_socket::getStats() const{
	return stats;
}

void si_socket::clearStats(void){
	memset(&stats, 0x00, sizeof(socket_stats));
}

string si_socket::toString() const {
	ostringstream retVal;
	retVal << "[ifname = \"" << ifname << "\"; ";
	retVal << "isOpen = " << isOpen() << "]";
	return retVal.str();
}

string si_socket::getName() const {
	return ifname;
}

