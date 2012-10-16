#include "rawutils.h"
#include "debug.h"
#include "fct_types.h"
#include "switched.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <arpa/inet.h>

using namespace std;


uint16_t bitMask(int bits){
	return 0xffff>>(16-bits);
}

string byteCountToString(uint64_t bytes){
	stringstream sstr;
	sstr.unsetf( ios::hex );
	sstr.setf( ios::dec );

	char prefixes[] = {0x00, 'K', 'M', 'G', 'T', 'P', 'E', 0x00};
	int index=0;
	uint64_t acc=0;

	while(prefixes[index+1] != 0x00){
		acc = bytes >> ((index+1)*10);

		if(acc==0){
			break;
		}
		index++;
	}

	if(prefixes[index] != 0x00){
		sstr << ((float)bytes/powf(1024,index)) << " " << prefixes[index] << "iB";
	}
	else{
		sstr << ((float)bytes/powf(1024,index)) << " " << prefixes[index] << "B";
	}

	return sstr.str();
}

string boolToString(bool val){
	if(val){
		return "true";
	}
	return "false";
}

void printBytes(uint8_t* buffer, int len){

	for(int i=0; i<len; i++){
		printf("0x%.2x, ", buffer[i]);
	}
	cout<<endl;
}

void printBits(uint8_t* buffer, int len){
	int bytes=len/8;
	int bits=len;

	if((len%8) != 0){
		bytes++;
	}

	for(int i=0; i<bytes; i++){
		for(int j=7; j>=0; j--){
			uint8_t buf=buffer[i];
			buf=buf>>j;
			buf=buf&0x01;

			if(buf==0){
				cout<<"0";
			}
			else{
				cout<<"1";
			}
		}

		cout<<" ";
	}
	cout<<endl;


	int bit=0;
	for(int i=0; i<bytes; i++){

		for(int j=7; j>=0; j--){
			cout<<j;
			bit++;
			if(bit>=bits){
				break;
			}
		}
		cout<<" ";
	}
	cout<<endl;
}

void bitCopy(bit_desc* dest, bit_desc* src, uint32_t len){
	uint8_t buf=0x00;
	uint8_t bits_read=0;

	uint32_t src_byte=src->byte;
	uint32_t src_bit=src->bit+1;
	uint32_t dest_byte=dest->byte;
	uint32_t dest_bits=dest->bit+1;

	while(len > 0){
		bits_read=src_bit;
		buf=src->buf[src_byte]&bitMask(bits_read);

		if(len < bits_read){
			buf=buf>>(bits_read-len);
			bits_read=len;
		}
		len-=bits_read;

		if(dest_bits < bits_read){
			dest->buf[dest_byte]&=(bitMask(8-dest_bits)<<(dest_bits));

			dest->buf[dest_byte]|=buf>>(bits_read-dest_bits);
			dest_byte++;

			bits_read-=dest_bits;
			dest_bits=8;

			dest->buf[dest_byte]&=bitMask(dest_bits-bits_read);
			dest->buf[dest_byte]|=(buf&bitMask(bits_read))<<(dest_bits-bits_read);

			dest_bits-=bits_read;
		}
		else{
			if(dest_bits < 8){
				dest->buf[dest_byte]&=(bitMask(8-dest_bits)<<(dest_bits)) | bitMask(dest_bits-bits_read);
			}
			else{
				dest->buf[dest_byte]&=bitMask(dest_bits-bits_read);
			}

			dest->buf[dest_byte]|=buf<<(dest_bits-bits_read);

			dest_bits-=bits_read;
			if(dest_bits<=0){
				dest_bits=8;
				dest_byte++;
			}
		}

		src_byte++;
		src_bit=8;
	}
}

address_chunk& address_chunk::operator=(const address_chunk &addr){
	this->size = addr.size;
	this->value = addr.value;

	return *this;
}

bool address_chunk::operator==(const address_chunk &other) const{
	return (size==other.size && value==other.value);
}

bool address_chunk::operator!=(const address_chunk &other) const{
	return (size!=other.size || value==other.value);
}

void address_chunk::operator++() {

	if(value >= 0x0fff){
		value = 1;
	}
	else{
		value++;
	}

	//Update size field
	if(value > 0x0ff){
		//12 bits are needed
		size = 12;
	}
	else if(value > 0x00f){
		//8 bits are needed
		size = 8;
	}
	else{
		size = 4;
	}
}

raw_address::raw_address(){
	buffer = NULL;
	tierLevel = 0;
	bytes = 0;
}

raw_address::raw_address(const raw_address& addr){
	this->tierLevel = addr.tierLevel;
	this->bytes = addr.countBytes();

	this->chunks = addr.chunks;
	this->extraData = addr.extraData;

	/*if(addr.buffer != NULL){
		this->buffer = new uint8_t[addr.bytes];
		memcpy(this->buffer, addr.buffer, this->bytes);
	}
	else{
		this->buffer = NULL;
		updateBuffer();
	}*/

	this->buffer = NULL;
	updateBuffer();
}

raw_address::raw_address(const si_address& addr){
	tierLevel=addr.bc;
	bytes = addr.byteCount();
	buffer = new uint8_t[bytes];

	//Copy chunks
	for(unsigned int i=0; i<addr.chunks.size(); i++){
		address_chunk c;
		c.value=addr.chunks.at(i);
		c.size=0;
		if(c.value <= 0xF){
			//c.size=0x01;
			c.size = 4;
		}
		else if(c.value <= 0xFF){
			//c.size=0x02;
			c.size = 8;
		}
		else if(c.value <= 0xFFF){
			//c.size=0x03;
			c.size = 12;
		}
		chunks.push_back(c);

		//DEBUG_MSG("c.size="<<(int)c.size<<" c.value="<<(int)c.value<<endl);
	}
	address_chunk termChunk;
	termChunk.size=0;
	termChunk.value=0;

	chunks.push_back(termChunk);

	//Copy Extra data
	for(unsigned int i=0; i<addr.extra.size(); i++){
		extraData.push_back(addr.extra.at(i));
	}

	writeAddress(buffer);
}

raw_address::raw_address(const uint8_t* buf){
	tierLevel=readBC(buf);
	chunks = readAllChunks(buf);

	/*bit_desc bitPtr={0, 1, buffer};
	address_chunk chunk={0,0};

	DEBUG_MSG("byte="<<bitPtr.byte << " bit=" << (int)bitPtr.bit << " c.size=" << (int)chunk.size*4 << " c.value=" << (int)chunk.value<<endl);
	while(readChunk(&bitPtr, &chunk) >= 0){
		chunks.push_back(chunk);
		DEBUG_MSG("byte="<<bitPtr.byte << " bit=" << (int)bitPtr.bit << " c.size=" << (int)chunk.size*4 << " c.value=" << (int)chunk.value<<endl);
	}

	bytes = bitPtr.byte+1;*/

	this->bytes = raw_address::byteCount(buf);
	this->buffer = new uint8_t[bytes];
	memcpy(this->buffer, buf, bytes);

	//TODO: Add support for reading extra bytes
}

raw_address::raw_address(uint8_t tier, vector<address_chunk> chunkVect){
	tierLevel=tier;
	chunks=chunkVect;
	buffer = NULL;
	bytes = 0;
	//TODO: Add support for reading extra bytes
}

raw_address::~raw_address(){
	if(buffer!=NULL){
		delete[] buffer;
		buffer = NULL;
	}
}

uint16_t raw_address::countBits() const {
	uint16_t bits = 6 + 12 + extraData.size()*8;

	vector<address_chunk>::const_iterator iter = chunks.begin();
	for(iter=chunks.begin(); iter != chunks.end(); iter++){
		bits += iter->size;
	}

	return bits;
}

uint16_t raw_address::countBytes() const {
	uint16_t bits = countBits();
	if((bits & 0x7) != 0){
		return (bits >> 3) + 1;
	}
	return (bits >> 3);
}

void raw_address::updateByteCount(){
	bytes = countBytes();
}

void raw_address::updateBuffer(){
	if(buffer != NULL){
		delete buffer;
	}

	updateByteCount();

	buffer = new uint8_t[bytes];

	writeAddress(buffer);
}

void raw_address::writeAddress(uint8_t* buf){
	updateByteCount();

	memset(buf, 0x00, bytes);
	buf[0] = (0x3f & tierLevel) << 2;

	bit_desc bitPtr={0, 1, buf};
	vector<address_chunk>::iterator iter = chunks.begin();

	/*DEBUG_MSG("Using "<<bytes<<" bytes for "<<chunks.size()<<" chunks and "<<extraData.size()<<" bytes of extra data"<<endl);

	int i=0;
	for(iter=chunks.begin(); iter != chunks.end(); iter++){
		DEBUG_MSG("Chunk["<<i<<"] size="<<iter->size*4<<" value="<<iter->value<<endl);
		i++;
	}*/

	for(iter=chunks.begin(); iter != chunks.end(); iter++){
		address_chunk chunk = *iter;

		//DEBUG_MSG("BEFORE byte="<<bitPtr.byte << " bit=" << (int)bitPtr.bit << " c.size=" << (int)chunk.size*4 << " c.value=" << (int)chunk.value<<endl);

		writeChunk(&bitPtr, &chunk);

		//DEBUG_MSG("AFTER  byte="<<bitPtr.byte << " bit=" << (int)bitPtr.bit << " c.size=" << (int)chunk.size*4 << " c.value=" << (int)chunk.value<<endl);
	}
}


raw_address& raw_address::operator=(const raw_address &addr){
	this->tierLevel=addr.tierLevel;
	this->chunks=addr.chunks;
	this->extraData=addr.extraData;
	this->bytes = addr.bytes;
	if(this->buffer != NULL){
		delete[] this->buffer;
		this->buffer = NULL;
	}

	if(addr.bytes > 0 && addr.buffer != NULL){
		this->buffer = new uint8_t[addr.bytes];
		memcpy(this->buffer, addr.buffer, addr.bytes);
	}
	else{
		this->buffer = NULL;
		updateBuffer();
	}
	return *this;
}

raw_address& raw_address::operator=(const si_address &addr){
	DEBUG_MSG("Setting to " << addr.toString() << endl);
	tierLevel=addr.bc;
	bytes = addr.byteCount();

	if(this->buffer != NULL){
		delete this->buffer;
	}
	this->buffer = new uint8_t[bytes];

	chunks.clear();
	//Copy chunks
	for(unsigned int i=0; i<addr.chunks.size(); i++){
		address_chunk c;
		c.value=addr.chunks.at(i);
		c.size=0;
		if(c.value <= 0xF){
			//c.size=0x01;
			c.size = 4;
		}
		else if(c.value <= 0xFF){
			//c.size=0x02;
			c.size = 8;
		}
		else if(c.value <= 0xFFF){
			//c.size=0x03;
			c.size = 12;
		}
		chunks.push_back(c);

		//DEBUG_MSG("c.size="<<(int)c.size<<" c.value="<<(int)c.value<<endl);
	}
	address_chunk termChunk;
	termChunk.size=0;
	termChunk.value=0;

	chunks.push_back(termChunk);

	extraData.clear();
	//Copy Extra data
	for(unsigned int i=0; i<addr.extra.size(); i++){
		extraData.push_back(addr.extra.at(i));
	}

	writeAddress(this->buffer);
	return *this;
}

bool raw_address::operator<(const raw_address &addr) const {
	//DEBUG_MSG(endl);
	if(this->tierLevel < addr.tierLevel){
		return true;
	}
	else if(this->tierLevel > addr.tierLevel){
		return false;
	}

	if(this->chunks.size() < addr.chunks.size()){
		return true;
	}
	else if(this->chunks.size() > addr.chunks.size()){
		return false;
	}

	for(unsigned int i=0; i<this->chunks.size(); i++){
		if(this->chunks[i].value < addr.chunks[i].value){
			return true;
		}
		if(this->chunks[i].value > addr.chunks[i].value){
			return false;
		}
	}

	return false;
}

bool raw_address::operator==(const raw_address &other) const {
	//DEBUG_MSG(endl);
	if(tierLevel==other.tierLevel){
		if(chunks.size()==other.chunks.size() && extraData.size()==other.extraData.size()){
			for(unsigned int i=0; i<chunks.size(); i++){
				if(this->chunks.at(i) == other.chunks.at(i)){
					continue;
				}
				return false;
			}

			for(unsigned int i=0; i<extraData.size(); i++){
				if(extraData.at(i) != other.extraData.at(i)){
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

bool raw_address::operator!=(const raw_address &other) const {
	//DEBUG_MSG(endl);
	return !(*this==other);
}

string raw_address::toString() const {
	stringstream sstr;
	sstr.unsetf( ios::dec );
	sstr.setf( ios::hex );

	sstr<<(uint32_t)tierLevel<<".";

	for(uint32_t i=0; i < chunks.size(); i++){
		if(chunks.at(i).size == 0){
			break;
		}

		sstr<<(uint32_t)chunks.at(i).value;
		if(i <= chunks.size()-2 && chunks.at(i+1).size){
			sstr<<":";
		}
	}

	if(extraData.size() > 0){
		sstr<<"::";
		for(uint32_t i=0; i<extraData.size(); i++){
			sstr<<(uint32_t)extraData.at(i);
		}
	}

	return sstr.str();
}

string raw_address::toString(const uint8_t* buffer, bool showSize){
	stringstream sstr;
	sstr.unsetf( ios::dec );
	sstr.setf( ios::hex );

	sstr.unsetf( ios::hex );
	sstr.setf( ios::dec );
	sstr<<(int)raw_address::readBC(buffer);
	sstr.unsetf( ios::dec );
	sstr.setf( ios::hex );

	bit_desc addrDesc={0, 1, (uint8_t*) buffer};
	address_chunk chunk;

	int chunkCount=0;
	while(readChunk(&addrDesc, &chunk) != -1){
		if(chunkCount==0){
			sstr<<".";
		}
		else{
			sstr<<":";
		}

		sstr<<chunk.value;
		if(showSize){
			sstr<<"("<<(int)chunk.size<<")";
		}
		chunkCount++;
	}
	return sstr.str();
}

uint8_t raw_address::readBC(const uint8_t* buffer){


	uint8_t bc=(uint8_t)buffer[0]>>2;
	return bc&0x3f;
}


int32_t raw_address::readChunk(const uint8_t* buffer, uint16_t index){
	int remaining=2;
	int offset=0;
	int count=-1;

	uint8_t size=0;
	uint16_t chunk=0;

	while(count!=index && offset<MAX_ADDRESS_LEN){
		if(remaining<=0){
			offset++;
			remaining=8;
		}

		// Calculate the Chunk size by adding with 00000011 to get the CS value and multiply it by 4
		// to get the number of bits to be read.
		size=((buffer[offset]>>(remaining-2))&0x03) * 4;
		remaining-=2;


		if(size==0){
			//At end of address chunks
			return -1;
		}

		chunk=0;
		while(size!=0){

			if(remaining<=0){
				remaining=8;
				offset++;
			}

			if(remaining<size){
				size-=remaining;
				chunk=chunk<<remaining;
				chunk|=buffer[offset] & bitMask(remaining);
				remaining=0;
			}
			else{
				chunk=chunk<<size;
				chunk|=(buffer[offset]>>(remaining-size)) & bitMask(size);
				remaining-=size;
				break;
			}
		}
		count++;
	}

	return (int32_t)chunk;
}

int32_t raw_address::readChunk(bit_desc* address, address_chunk* chunk){
	chunk->value=0;
	if(address->bit>1){
		chunk->size=(address->buf[address->byte]>>(address->bit-1)) & 0x03;
		address->bit-=2;
	}
	else{
		chunk->size=address->buf[address->byte] & 0x03;
		address->bit=7;
		address->byte++;
	}
	chunk->size=chunk->size*4;

	if(chunk->size==0){
		return -1;
	}

	int read_bits=0;
	//int read_remaining=chunk->size;
	while(read_bits < chunk->size){

		int bitsInByte=address->bit+1;
		int read_remaining=chunk->size-read_bits;

		if(read_remaining >= bitsInByte){	//Need to read more bits than are remaining in this byte
			chunk->value=chunk->value<<bitsInByte;
			chunk->value|=(uint16_t) (address->buf[address->byte] & bitMask(bitsInByte));
			read_bits+=bitsInByte;
			address->bit=7;
			address->byte++;
		}
		else{
			chunk->value=chunk->value<<read_remaining;
			chunk->value|=(uint16_t)(address->buf[address->byte] >> (bitsInByte-read_remaining)) & bitMask(read_remaining);

			read_bits+=bitsInByte;
			address->bit-=(read_remaining);
		}
	}

	return (int32_t)chunk->value;
}

int32_t raw_address::peekChunk(bit_desc address){
	address_chunk chunk;
	chunk.value=0;
	if(address.bit > 1){
		chunk.size=(address.buf[address.byte]>>(address.bit-1)) & 0x03;
		address.bit-=2;
	}
	else{
		chunk.size=address.buf[address.byte] & 0x03;
		address.bit=7;
		address.byte++;
	}


	chunk.size=chunk.size << 2;	//Multiply by four

	if(chunk.size==0){
		return -2;
	}

	int read_bits=0;
	//int read_remaining=chunk.size;
	while(read_bits < chunk.size){

		int bitsInByte=address.bit+1;
		int read_remaining=chunk.size-read_bits;

		if(read_remaining >= bitsInByte){	//Need to read more bits than are remaining in this byte
			chunk.value=chunk.value<<bitsInByte;
			chunk.value|=(uint16_t) (address.buf[address.byte] & bitMask(bitsInByte));
			read_bits+=bitsInByte;
			address.bit=7;
			address.byte++;
		}
		else{
			chunk.value=chunk.value<<read_remaining;
			chunk.value|=(uint16_t)(address.buf[address.byte] >> (bitsInByte-read_remaining)) & bitMask(read_remaining);

			read_bits+=bitsInByte;
			address.bit-=(read_remaining);
		}
	}


	return readChunk(&address, &chunk);
}

void raw_address::writeChunk(bit_desc* address, address_chunk* chunk){
	//Note: chunkValue contains the 2bit size field infront of the chunk value
	//	thus bitsToWrite is the size if the chunk's value field plus the
	//	number of bits for the size field which is always 2 and bitsToWrite
	//	is at most 14 and is atleast 2 if size=0

	//DEBUG_MSG("Writing "<<chunk->value<<"("<<(int)chunk->size<<") to "<<(int)address->byte<<":"<<(int)address->bit<<endl);
	int bitsToWrite=(chunk->size)+2;
	uint16_t chunkValue=chunk->value | ((chunk->size/4)<<((chunk->size)));

	//printBits((uint8_t*)&chunkValue, 16);

	while(bitsToWrite>0){
		int bitsFree=address->bit+1;

		//DEBUG_MSG("bitsFree="<<bitsFree<<" bitsToWrite="<<bitsToWrite<<endl);
		//printAddressBits(address->buf);

		if(bitsToWrite >= bitsFree){
			//DEBUG_MSG("At "<<(int)address->byte<<":"<<(int)address->bit<<" write "<<bitsFree<<"bits"<<endl);
			address->buf[address->byte]&=(bitMask(8-bitsFree)<<bitsFree);
			address->buf[address->byte]|=(chunkValue >> (bitsToWrite-bitsFree)) & bitMask(bitsFree);

			bitsToWrite-=bitsFree;
			address->bit=7;
			address->byte++;

		}
		else{
			//DEBUG_MSG("At "<<(int)address->byte<<":"<<(int)address->bit<<" write "<<bitsToWrite<<"bits"<<endl);
			address->buf[address->byte]&=(bitMask(8-bitsFree)<<bitsFree) | (bitMask(bitsFree-bitsToWrite));
			address->buf[address->byte]|=(bitMask(bitsToWrite)&chunkValue)<<(bitsFree-bitsToWrite);

			address->bit=(bitsFree-bitsToWrite)-1;
			bitsToWrite=0;
		}
	}
}


void raw_address::writeExtraData(bit_desc* bitPtr, uint16_t length, uint8_t* data){
	//TODO: IMPLEMENT
	UNSUPPORTED_MSG("NEED TO IMPLEMENT"<<endl);
}


uint16_t raw_address::appendChunk(uint8_t* buffer, uint16_t value){
	bit_desc bitPtr = {0, 1, buffer};
	address_chunk chunk;

	//Read to the last chunk
	while(raw_address::peekChunk(bitPtr) > -2){
		raw_address::readChunk(&bitPtr, &chunk);
	}

	//TODO: SUPPORT EXTRA, Copy extra data to temporary buffer

	chunk.value = value;

	if(chunk.value > 0x0ff){
		//12 bits are needed
		chunk.size = 12;
	}
	else if(chunk.value > 0x00f){
		//8 bits are needed
		chunk.size = 8;
	}
	else{
		chunk.size = 4;
	}


	//Write the desired chunk
	writeChunk(&bitPtr, &chunk);

	//Terminate the chunk list
	chunk.size = 0;
	chunk.value = 0;
	writeChunk(&bitPtr, &chunk);

	//TODO: This is VERY slow, make this faster later!
	//Zero out the extra length field
	for(int i=0; i<6; i++){
		writeChunk(&bitPtr, &chunk);
	}

	//TODO: SUPPORT EXTRA, Copy extra data from temporary buffer and back into address

	//Return the length of the appended
	return bitPtr.byte + 1;
}



vector<address_chunk> raw_address::readAllChunks(const uint8_t* buffer){
	bit_desc bitPtr={0, 1, (uint8_t*)buffer};
	address_chunk chunk={0,0};
	vector<address_chunk> chunks;

	while(readChunk(&bitPtr, &chunk) > 0){
		chunks.push_back(chunk);
	}
	chunks.push_back(chunk);
	return chunks;
}

uint16_t raw_address::chunkCount(const uint8_t* buffer){
	int remaining=2;
	int offset=0;
	int count=0;
	uint8_t size=0;

	while(offset<MAX_ADDRESS_LEN){
		if(remaining<=0){
			offset++;
			remaining=8;
		}

		size=((buffer[offset]>>(remaining-2))&0x03) * 4;
		remaining-=2;


		if(size==0){
			//At end of address chunks
			break;
		}

		while(size!=0){

			if(remaining<=0){
				remaining=8;
				offset++;
			}

			if(remaining<size){
				size-=remaining;
				offset++;
				remaining=8;
			}
			else{
				remaining-=size;
				break;
			}
		}
		count++;
	}

	return count;
}

uint16_t raw_address::byteCount(const uint8_t *buffer){
	uint16_t count=bitCount(buffer);

	if(count%8 != 0){
		return (count/8)+1;
	}

	return count/8;
}

uint16_t raw_address::bitCount(const uint8_t* buffer){
	int remaining=2;
	int offset=0;
	uint8_t size=0;

	while(offset<MAX_ADDRESS_LEN){
		if(remaining<=0){
			offset++;
			remaining=8;
		}

		size=((buffer[offset]>>(remaining-2))&0x03) * 4;
		remaining-=2;


		if(size==0){
			//At end of address chunks
			/*size=4;
			while(size!=0){
				if(remaining<=0){
					remaining=8;
					offset++;
				}

				if(remaining<size){
					size-=remaining;
					offset++;
					remaining=8;
					continue;
				}

				remaining-=size;
				break;
			}*/
			break;
		}

		while(size!=0){

			if(remaining<=0){
				remaining=8;
				offset++;
			}

			if(remaining<size){
				size-=remaining;
				offset++;
				remaining=8;
				continue;
			}

			remaining-=size;
			break;
		}
	}

	if(offset >= MAX_ADDRESS_LEN){
		ERROR("Address longer than maximum allowed length!"<<endl);
	}

	if(remaining<=0){
		offset++;
		remaining=8;
	}

	//Read extra length
	size=12;

	while(size!=0){
		if(remaining<=0){
			remaining=8;
			offset++;
		}

		if(remaining<size){
			size-=remaining;
			offset++;
			remaining=8;
			continue;
		}

		remaining-=size;
		break;
	}


	return ((offset+1)*8)-remaining;
}


int32_t raw_address::removeChunk(uint8_t* buffer, uint16_t index, bool packRight){
	int remaining=2;
	int offset=0;
	int writeByte=-1;
	int writeRemaining=-1;
	int count=0;
	uint8_t chunkBits=0;


	while(offset<MAX_ADDRESS_LEN){
		if(remaining<=0){
			offset++;
			remaining=8;
		}

		if(count==index){
			writeByte=offset;
			writeRemaining=remaining;
		}

		int chunkSize=((buffer[offset]>>(remaining-2))&0x03);
		chunkBits=chunkSize * 4;
		remaining-=2;

		if(chunkBits==0){
			if(count==index){
				writeByte=-1;
			}

			//At end of address chunks
			if(writeByte!=-1 && count>index){	//Write Chunk
				if(writeRemaining<=0){
					writeByte++;
					writeRemaining=8;
				}

				buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
				buffer[writeByte]|=(chunkSize&0x03)<<(writeRemaining-2);
				writeRemaining-=2;
			}
			break;
		}

		uint16_t chunk=0;
		while(chunkBits!=0){

			if(remaining<=0){
				remaining=8;
				offset++;
			}

			if(remaining<chunkBits){
				chunkBits-=remaining;
				chunk=chunk<<remaining;
				chunk|=buffer[offset] & bitMask(remaining);
				offset++;
				remaining=8;
			}
			else{
				chunk=chunk<<chunkBits;
				chunk|=(buffer[offset]>>(remaining-chunkBits)) & bitMask(chunkBits);
				remaining-=chunkBits;
				break;
			}
		}


		if(writeByte!=-1 && count>index){	//Write Chunk
			if(writeRemaining<=0){
				writeByte++;
				writeRemaining=8;
			}

			chunkBits=chunkSize*4;
			if(writeRemaining>=2){
				buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
				buffer[writeByte]|=(chunkSize&0x03)<<(writeRemaining-2);
				writeRemaining-=2;

				while(chunkBits!=0){
					if(writeRemaining<=0){
						writeByte++;
						writeRemaining=8;
					}

					if(writeRemaining<chunkBits){
						buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
						buffer[writeByte]|= (chunk>>(chunkBits-writeRemaining)) & bitMask(writeRemaining);
						chunkBits-=writeRemaining;
						writeByte++;
						writeRemaining=8;
					}
					else{
						buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
						buffer[writeByte]|=(bitMask(chunkBits) & chunk) << (writeRemaining-chunkBits);
						writeRemaining-=chunkBits;
						chunkBits=0;
					}
				}
			}
			else{
				ERROR("No space for chunk size!");
				return -1;
			}
		}

		count++;
	}


	if(remaining<=0){
		offset++;
		remaining=8;
	}

	if(writeByte!=-1){	//Write Extras
		if(writeRemaining<=0){
			writeByte++;
			writeRemaining=8;
		}

		uint16_t chunk=0;
		chunkBits=12;

		while(chunkBits!=0){
			if(writeRemaining<=0){
				writeByte++;
				writeRemaining=8;
			}

			if(writeRemaining<chunkBits){
				buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
				buffer[writeByte]|=(chunk>>(chunkBits-writeRemaining)) & bitMask(writeRemaining);
				chunkBits-=writeRemaining;
				writeByte++;
				writeRemaining=8;
			}
			else{
				buffer[writeByte]&=bitMask(8-writeRemaining)<<writeRemaining;
				buffer[writeByte]|=(bitMask(chunkBits) & chunk) << (writeRemaining-chunkBits);
				writeRemaining-=chunkBits;
				chunkBits=0;
				break;
			}
		}
	}

	if(writeByte<0){
		return 0;
	}

	int bitsRemoved=(offset+1)*8-remaining+12;
	bitsRemoved-=(writeByte+1)*8-writeRemaining;
	return bitsRemoved;
}


void raw_address::incrementTier(uint8_t* buffer, int8_t amount){
	uint8_t bc=((int8_t)raw_address::readBC(buffer) + amount);
	buffer[0]&=bitMask(2);
	buffer[0]|=bc<<2;
}


void raw_address::setTier(uint8_t* buffer, uint8_t value){
	buffer[0]&=bitMask(2);
	buffer[0]|=value<<2;
}


void raw_address::minimizeAddress(uint8_t* target, uint8_t* host, uint8_t* buffer){
	bit_desc targetDesc={0, 1, target};
	address_chunk targetChunk;
	bit_desc hostDesc={0, 1, host};
	address_chunk hostChunk;
	bit_desc destDesc={0, 1, buffer};

	int newBC=0;

	while(readChunk(&targetDesc, &targetChunk)!=-1 && readChunk(&hostDesc, &hostChunk)!=-1){
		if(targetChunk.size != hostChunk.size){
			break;
		}
		if(targetChunk.value != hostChunk.value){
			break;
		}

		//DEBUG_MSG("Matched value="<<(int)targetChunk.value<<" size="<<(int)targetChunk.size<<endl);

		newBC++;
	}

	setTier(buffer, newBC);
	//writeChunk(&destDesc, &targetChunk);

	if(targetChunk.size!=0 && buffer != target){
		//DEBUG_MSG("Adding leftovers"<< endl);

		do{
			//DEBUG_MSG("Adding value="<<(int)targetChunk.value<<" size="<<(int)targetChunk.size<<endl);
			writeChunk(&destDesc, &targetChunk);
		}
		while(readChunk(&targetDesc, &targetChunk)!=-1);
	}

	//TODO: Copy extra bits over!
}


uint8_t raw_address::getRelationship(uint8_t* target, uint8_t* host){
	if(target == NULL || host==NULL){
		return (uint8_t) INVALID;
	}

	if(raw_address::readBC(target) == RESERVED_BC){
		return (uint8_t) INVALID;
	}

	uint8_t minAddr[100];
	int tierDelta=0;
	uint16_t chunks=0;
	uint8_t minBC=0;
	uint8_t local_bc = raw_address::readBC(host);

	memset(minAddr, 0x00, 100);

	raw_address::minimizeAddress(target, host, minAddr);

	minBC = raw_address::readBC(minAddr);
	tierDelta = minBC - local_bc;
	chunks = raw_address::chunkCount(minAddr);

	/*DEBUG_MSG("Debug\t Local=" << raw_address::toString(host)
			  << "\tTarget=" << raw_address::toString(target)
			  << "\tMinAddr=" << raw_address::toString(minAddr) << endl);

	raw_address::printAddressBytes(host);
	raw_address::printAddressBytes(target);
	raw_address::printAddressBytes(minAddr);

	DEBUG_MSG("minBC="<<(int)minBC<<endl);
	DEBUG_MSG("chunks="<<(int)chunks<<endl);*/

	if(minBC == local_bc){
		if(chunks == 0){
			return (uint8_t) INTERNAL;
		}
		else if(chunks == 1){
			return (uint8_t) DOWNLINK;
		}

		return (uint8_t) (DOWNLINK | IMPROPER);
	}
	else if(minBC == (local_bc - 1)){
		if(chunks == 0){
			return (uint8_t) UPLINK;
		}
		else if(chunks == 1){
			return (uint8_t) TRUNK;
		}

		return (uint8_t) (DOWNLINK | IMPROPER);
	}

	chunks+=minBC;

	if(chunks == local_bc){
		return (uint8_t) (TRUNK | IMPROPER);
	}
	else if(chunks < local_bc){
		return (uint8_t) (UPLINK | IMPROPER);
	}

	//Should never hit this case
	return (uint8_t) (DOWNLINK | IMPROPER);
}

bool raw_address::compareAddress(uint8_t* addr1, uint8_t* addr2){
	if(raw_address::readBC(addr1) == raw_address::readBC(addr2)){
		address_chunk chunk1;
		address_chunk chunk2;

		bit_desc addrDesc1 = {0, 1, addr1};
		bit_desc addrDesc2 = {0, 1, addr2};

		int32_t size1 = readChunk(&addrDesc1, &chunk1);
		int32_t size2 = readChunk(&addrDesc2, &chunk2);

		while(size1 != -1 && size2 != -1){
			if(size1 != size2){
				return false;
			}

			if(chunk1.value != chunk2.value){
				return false;
			}

			size1 = readChunk(&addrDesc1, &chunk1);
			size2 = readChunk(&addrDesc2, &chunk2);
		}

		if(size1!=size2){
			return false;
		}

		return true;
	}

	return false;
}

void raw_address::printAddress(uint8_t *buffer, bool showSize){
	cout.unsetf( ios::hex );
	cout.setf( ios::dec );
	cout<<(int)raw_address::readBC(buffer);
	cout.unsetf( ios::dec );
	cout.setf( ios::hex );

	bit_desc addrDesc={0, 1, buffer};
	address_chunk chunk;

	int chunkCount=0;
	while(readChunk(&addrDesc, &chunk) != -1){
		if(chunkCount==0){
			cout<<".";
		}
		else{
			cout<<":";
		}

		cout<<chunk.value;
		if(showSize){
		    cout<<"("<<(int)chunk.size<<")";
		}
		chunkCount++;
	}
	cout.setf( ios::dec );
}

void raw_address::printAddressBits(uint8_t* buffer, bool annotate){
	int bytes=raw_address::byteCount(buffer);
	int bits=raw_address::bitCount(buffer);

	for(int i=0; i<bytes; i++){

		for(int j=7; j>=0; j--){
			uint8_t buf=buffer[i];
			buf=buf>>j;
			buf=buf&0x01;

			if(buf==0){
				cout<<"0";
			}
			else{
				cout<<"1";
			}
		}

		cout<<" ";
	}
	cout<<endl;


	int bit=0;
	for(int i=0; i<bytes; i++){

		for(int j=7; j>=0; j--){
			cout<<j;
			bit++;
			if(bit>=bits){
				break;
			}
		}
		cout<<" ";
	}
	cout<<endl;

	if(annotate){
		//
	}
}

void raw_address::printAddressBytes(uint8_t* buffer){
	int bytes=raw_address::byteCount(buffer);

	for(int i=0; i<bytes; i++){
		printf("0x%.2x, ", buffer[i]);
	}
	cout<<endl;
}


string addressListToString(vector<raw_address> addresses){
	ostringstream sstr;

	sstr<<"{";

	for(unsigned int i=0; i<addresses.size(); i++){
		sstr<<addresses.at(i).toString();
		if(i != (addresses.size()-1)){
			sstr<<", ";
		}
	}
	sstr<<"}";
	return sstr.str();
}

string addressListToString(vector<uint8_t*> addresses){
	ostringstream sstr;

	sstr<<"{";

	for(unsigned int i=0; i<addresses.size(); i++){
		sstr << raw_address::toString(addresses.at(i));
		if(i != (addresses.size()-1)){
			sstr<<", ";
		}
	}
	sstr<<"}";
	return sstr.str();
}

string addressListToString(vector<raw_address*> addresses){
	ostringstream sstr;

	sstr<<"{";

	for(unsigned int i=0; i<addresses.size(); i++){
		sstr << addresses.at(i)->toString();
		if(i != (addresses.size()-1)){
			sstr<<", ";
		}
	}
	sstr<<"}";
	return sstr.str();
}

string addressListToString(list<raw_address*> addresses){
	ostringstream sstr;

	sstr<<"{";

	list<raw_address*>::iterator addrIter = addresses.begin();

	for(unsigned int i=0; addrIter != addresses.end(); addrIter++, i++){
		raw_address* addr = *addrIter;
		sstr << addr->toString();
		if(i != (addresses.size()-1)){
			sstr<<", ";
		}
	}
	sstr<<"}";
	return sstr.str();
}

raw_packet::raw_packet(){
	this->offset=0;
	this->length=0;
	this->rx_socket=NULL;
	this->tx_socket=NULL;
	memset(data, 0x00, PKT_DST_OFFSET);
}

raw_packet::raw_packet(si_packet& packet){
	this->offset=0;
	this->length=packet.byteCount();
	packet.writeHeaders();
	memcpy(this->data, packet.getHeaderPtr(), packet.byteCount());
	this->rx_socket=NULL;
	this->tx_socket=NULL;
}

raw_packet::raw_packet(raw_packet& other){
	this->offset=other.offset;
	this->length=other.length;
	memcpy(this->data, &other.data[offset], length);
	this->rx_socket=NULL;
	this->tx_socket=NULL;
}

void raw_packet::setPacketType(uint16_t type){
	*((uint16_t*)(data+PKT_TYPE_OFFSET)) = htons(type);
}

uint16_t raw_packet::readPacketType() const{
	return ntohs(*(uint16_t*)(data+PKT_TYPE_OFFSET));
}

uint8_t raw_packet::readMMTPacketType() const{
	return data[length-MMT_PKT_TYPE_OFFSET];
}

void raw_packet::setPayloadLength(uint16_t len){
	int oldLen=(int)readPayloadLength();
	*((uint16_t*)(data+PKT_LEN_OFFSET)) = htons(len);
	length -= oldLen;
	length += len;
}

uint16_t raw_packet::readPayloadLength() const{
	return ntohs(*(uint16_t*)(data+PKT_LEN_OFFSET));
}

uint16_t raw_packet::readMMTPayloadLength() const {
	return ntohs(*(uint16_t*)(data+(length-MMT_PKT_LEN_OFFSET)));
}

uint8_t* raw_packet::destinationAddrPtr() const{
	return (uint8_t*)data+PKT_DST_OFFSET;
}

uint8_t* raw_packet::senderAddrPtr() const{
	uint8_t* dstPtr=destinationAddrPtr();
	int dstLen=raw_address::byteCount(dstPtr);
	return dstPtr+dstLen;
}

uint8_t* raw_packet::payloadPtr() const{
	uint8_t* sourcePtr=senderAddrPtr();
	int sourceLen=raw_address::byteCount(sourcePtr);
	return sourcePtr+sourceLen;
}

uint8_t* raw_packet::mmtPayloadPtr() const{
	return (uint8_t*)( (data+length) - (readMMTPayloadLength() + MMT_PKT_LEN_OFFSET));
}

uint8_t* raw_packet::dataPtr() const{
	return (uint8_t*)data+offset;
}

bool raw_packet::appendData(uint8_t* buf, uint16_t len){
	uint8_t* payload=payloadPtr();
	uint16_t payloadLen=readPayloadLength();

	if((length+len) < (ETH_FRAME_LEN-offset)){	//Ensure there is space
		if(buf != NULL){
			memcpy(&payload[payloadLen], buf, len);
		}
		setPayloadLength(payloadLen+len);
		return true;
	}
	return false;
}

bool raw_packet::shrinkPayload(uint16_t len){
	uint16_t payloadLen=readPayloadLength();

	if(len <= payloadLen){
		setPayloadLength(payloadLen - len);
		return true;
	}

	//Shrinking would cause a buffer underrun
	return false;
}

void raw_packet::clear(){
	length=0;
	offset=0;
	memset(data, 0x00, PKT_DST_OFFSET);
	tx_socket=NULL;
	rx_socket=NULL;
}

uint8_t* raw_packet::init(uint8_t* dest, uint8_t* src, uint16_t len){
	uint8_t* dataPtr = this->destinationAddrPtr();
	int destAddrLen = raw_address::byteCount(dest);
	int srcAddrLen = raw_address::byteCount(src);

	offset=0;
	tx_socket=NULL;
	rx_socket=NULL;
	//rx_link=NULL;

	//Clear Ethernet and SI headers
	memset(data, 0x00, PKT_DST_OFFSET);

	//Set destination and source addresses
	memcpy(dataPtr, dest, destAddrLen);
	dataPtr += destAddrLen;
	memcpy(dataPtr, src, srcAddrLen);
	dataPtr += srcAddrLen;

	//Initialize packet's buffer length
	this->length = ETH_HLEN + SI_PACKET_TYPE_SIZE + SI_PACKET_DATA_LEN_SIZE + destAddrLen + srcAddrLen;

	setPayloadLength(len);

	if(len > 0){
		//Clear payload
		memset(dataPtr, 0x00, len);
	}

	return dataPtr;
}

bool raw_packet::removeFirstDestAF(){
	uint8_t* destAddr=destinationAddrPtr();

	int destAddrLen=raw_address::byteCount(destAddr);
	int removedBits=raw_address::removeChunk(destAddr, 0);
	int bytesFree=destAddrLen-raw_address::byteCount(destAddr);
	//cout<<"Removed "<<removedBits<<"bits"<<endl;

	if(removedBits<0){
		return false;
	}
	else if(bytesFree>0){
		int bytesToMove=PKT_DST_OFFSET+(destAddrLen-bytesFree)-1;

		//cout<<"Have "<<bytesFree<<"bytes free and "<<bytesToMove<<"bytes to move"<<endl;

		//TODO: The following could be optimized more by copying 4bytes at a time, or using memmove
		for(int i=bytesToMove; i>=offset; i--){
			//printf("0x%.2x -> 0x%.2x ", data[i], data[i+bytesFree]);
			data[i+bytesFree]=data[i];
			//cout<<"Data @ "<<i<<" is now at "<<i+bytesFree<<endl;
		}
		offset+=bytesFree;
		length-=bytesFree;
	}
	return true;
}


void raw_packet::printPacket() const{

	//cout.unsetf(ios::dec);
	//cout.setf(ios::hex);

	//Print ETH_HDR
	for(int i=0; i<ETH_HLEN; i++){
		//cout<<(int)data[offset+i];
		printf("%.2x", data[offset+i]);
	}

	//cout<<"|";

	//Print SI_Type
	//cout<<(int)readPacketType()<<"|";
	printf("|%.2x%.2x|", data[PKT_TYPE_OFFSET], data[PKT_TYPE_OFFSET+1]);


	//Print Payload length
	//cout<<(int)readPayloadLength()<<"|";
	printf("%.2x%.2x|", data[PKT_LEN_OFFSET], data[PKT_LEN_OFFSET+1]);

	//Print Destination
	int destLen=raw_address::byteCount(destinationAddrPtr());
	for(int i=0; i<destLen; i++){
		//cout<<(int)data[offset+i+PKT_DST_OFFSET];
		printf("%.2x", data[i+PKT_DST_OFFSET]);
	}

	//cout<<"|";
	printf("|");

	//Print Source address
	int sourceLen=raw_address::byteCount(senderAddrPtr());
	for(int i=0; i<sourceLen; i++){
		printf("%.2x", data[i+PKT_DST_OFFSET+destLen]);
	}

	//cout<<"|";
	printf("|");

	for(int i=0; i<readPayloadLength(); i++){
		printf("%.2x", data[i+PKT_DST_OFFSET+destLen+sourceLen]);
	}

	//cout.setf(ios::dec);

}

raw_packet& raw_packet::operator=(si_packet& other){
	this->offset=0;
	this->length=other.byteCount();
	other.writeHeaders();
	memcpy(this->data, other.getHeaderPtr(), other.byteCount());
	this->rx_socket=NULL;
	this->tx_socket=NULL;
	return *this;
}

raw_packet& raw_packet::operator=(const raw_packet& other){
	this->offset=other.offset;
	this->length=other.length;
	memcpy(this->data, &other.data[offset], length);
	this->rx_socket=NULL;
	this->tx_socket=NULL;
	return *this;
}








