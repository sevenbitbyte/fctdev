#ifndef RAWUTILS_H
#define RAWUTILS_H

#include "switched.h"

#include <vector>
#include <string>
#include <inttypes.h>
#include <linux/if_ether.h>
#include <sys/time.h>

using namespace std;

class si_address;
class si_packet;
class si_socket;
class link_info;

#define MAX_ADDRESS_LEN 200

/**
  *	Sets the specified number of bits, starting
  *	with bit index 0.
  *
  *	@param	bits	Number of bits to set to 1
  */
uint16_t bitMask(int bits);

void printBits(uint8_t* buffer, int len);
void printBytes(uint8_t* buffer, int len);

string boolToString(bool val);

/**
  *	Writes a byte count to string using a reasonable prefix.
  *	Supports counting up to eight exabytes
  *
  *	@param	bytes
  *	@return Returns a string representing the number of bytes
  *			using the correct SI prefix
  */
string byteCountToString(uint64_t bytes);

/**
  *	@struct	bit_desc
  *	Bit Descriptor used to describe the start of a bit buffer
  */
struct bit_desc{
	uint32_t byte;

	/**	uint8_t	bit
	  *	Points to a specific bit, range is 0-7
	  */
	uint8_t bit;

	/**	uint8_t* buf
	  *	Points to the data buffer to work with
	  */
	uint8_t* buf;
};

//! Represents an address chunk
struct address_chunk{
	/**	@var uint8_t size
	  *	@brief	Bits used to store chunk
	  */
	uint8_t size;

	/**	@var uint16_t value
	  *	@brief	Value stored in chunk
	  */
	uint16_t value;

	address_chunk& operator=(const address_chunk &addr);
	bool operator==(const address_chunk &other) const;
	bool operator!=(const address_chunk &other) const;
	void operator++();
};

/**
  *	Copys len bits from src to dest.
  *	@param	dest	Destination to copy data to
  *	@param	src	Source to read from
  *	@param	len	Number of bits to copy from src
  */
void bitCopy(bit_desc* dest, bit_desc* src, uint32_t len);

//! Class for quick address parsing and light storage of address data
class raw_address{
	public:

		/** Construct blank address
		  */
		raw_address();

		/**
		  *	Copy constructor
		  */
		raw_address(const raw_address& addr);

		/** Copies an si_address. Supports extra chunks
		  * @param  addr    Address to copy
		  */
		raw_address(const si_address& addr);

		/** Construct address by Parsing the address stored in buffer. DOES NOT SUPPORT extra chunks
		  * @param  buffer  Buffer to parse
		  */
		raw_address(const uint8_t* buffer);

		/** Constructs address using supplied tier value and chunks
		  * @param  tier        Tier value
		  * @param  chunkVect   Vector of address chunks
		  */
		raw_address(uint8_t tier, vector<address_chunk> chunkVect);

		~raw_address();

		raw_address& operator=(const raw_address &addr);

		raw_address& operator=(const si_address &addr);

		bool operator<(const raw_address &addr) const;

		//! Test for equality between two addresses
		bool operator==(const raw_address &other) const;

		//! Test for inequality between two addresses
		bool operator!=(const raw_address &other) const;

		uint16_t countBits() const;

		uint16_t countBytes() const;

		/**
		  *	Updates the internal byte count. This should be called
		  *	if the chunks vector has been modified and you wish to read from
		  *	the raw buffer.
		  */
		inline void updateByteCount();

		/**
		  *	Updates the raw internal address buffer. This should be called
		  *	if the chunks vector has been modified and you wish to read from
		  *	the raw buffer.
		  */
		void updateBuffer();

		void writeAddress(uint8_t* buf);

		string toString() const;

		static string toString(const uint8_t* buffer, bool showSize=false);

		uint16_t bytes;
		uint8_t tierLevel;
		uint8_t* buffer;
		vector<address_chunk> chunks;
		vector<uint8_t> extraData;

		/**	Reads the BC value of the address
		  *	@param	buffer	Buffer address is stored in
		  *	@return	Returns the BC value as an unsigned char
		  */
		static uint8_t readBC(const uint8_t* buffer);

		/**
		  *	Parses the chunk at the given index
		  *	@param	buffer	Buffer address is stored in
		  *	@param	index	Index of chunk to read
		  *	@return Returns the value of the chunk at the given index
		  *		or -1 if an decoding error occurs
		  */
		static int32_t readChunk(const uint8_t* buffer, uint16_t index=0);


		/**
		  *	Parses the chunk starting at the bit described by bitAddress and
		  *	updates bitAddress to point to the next address chunk/field.
		  *	To read subsequent chunks, bitAddress can be passed unchange by the
		  *	calling function.
		  *
		  *	@param	bitAddress	Description of where to start parsing
		  *	@param	chunk		Will contain the value and size of the parsed chunk
		  *	@return	Returns the value of the chunk or -1 if the chunk size equals 0
		  */
		static int32_t readChunk(bit_desc* bitAddress, address_chunk* chunk);


		/**
		  *	Reads all address chunks and places them into a vector
		  *
		  *	@param	buffer
		  *	@return	Returns all of the chunks parsed from the provided buffer in
		  *			a vector.
		  */
		static vector<address_chunk> readAllChunks(const uint8_t* buffer);

		/**
		  *	Parses the chunk following the one pointed to by the supplied bit
		  *	descriptor.
		  *
		  *	@param	bitAddress	Description of where to start parsing
		  *	@return	Returns the value of the chunk or -1 if the size is equal to 0.
		  *			Returns -2 if the chunk pointed to be bitAddress has a size equal
		  *			to 0.
		  */
		static int32_t peekChunk(bit_desc bitAddress);

		/**
		  *	Writes the chunk supplied into the location pointed to by address and
		  *	updates address to point to the bit directly after the written chunk.
		  *	The 2bit chunk size is written first followed by chunk.size bits of
		  *	chunk.value
		  *
		  *	@param	address	Description of where to start chunk writing
		  *	@param	chunk	Chunk to write into address
		  */
		static void writeChunk(bit_desc* address, address_chunk* chunk);

		static void writeExtraData(bit_desc* bitPtr, uint16_t length, uint8_t* data=NULL);

		/**
		  *	Appends the supplied chunk value to the end of the supplied raw
		  *	address buffer.
		  *
		  *	@param	buffer	Address buffer
		  *	@param	value	Chunk value to append
		  *
		  *	@return	Returns the number of bytes used to store the address,
		  *			includes the space required for extra fields
		  */
		static uint16_t appendChunk(uint8_t* buffer, uint16_t value);

		/**
		  *     Counts the number of address chunks in the address buffer
		  *
		  *     @param  buffer  Address buffer
		  *     @return Returns the number of address chunks found in buffer
		  */
		static uint16_t chunkCount(const uint8_t* buffer);

		/**
		  *     Counts the number of bits required to represent the address stored in buffer
		  *
		  *     @param  buffer  Address buffer
		  *     @return Returns the minimum number of bits needed to store the address in buffer
		  */
		static uint16_t bitCount(const uint8_t* buffer);

		/**
		  *	Counts the number of bytes used to store the address in buffer
		  *
		  *	@param	buffer	Address buffer
		  *	@return	Returns the number of bytes used by the address in buffer
		  */
		static uint16_t byteCount(const uint8_t* buffer);

		/**
		  *	Removes the chunk located at the given index
		  *
		  *	@param	buffer	Buffer address is stored in
		  *	@param	index	Index of address chunk to remove
		  *	@param	packRight	If true and if removing the specified chunk
		  *						shortens the address by a byte the address will
		  *						be shifted to the right by a byte
		  *
		  *	@return	Returns the number of bits removed, -1 indicates a decode error
		  *			occured during address parsing.
		  */
		static int32_t removeChunk(uint8_t* buffer, uint16_t index=0, bool packRight=true);

		/**
		  *	Increments the tier level of the address in buffer by the specified amount
		  *
		  *	@param	buffer	Address buffer
		  *	@param	amount	Amount to increment tier value by, defaults to 1
		  */
		static void incrementTier(uint8_t* buffer, int8_t amount=1);

		/**
		  *	Sets the tier level of the address in buffer to the specified value
		  *
		  *	@param	buffer	Address buffer
		  *	@param	value	New tier value
		  */
		static void setTier(uint8_t* buffer, uint8_t value=0);

		/**
		  *	Minimizes the target address relative to host address
		  *	and stores the result in buffer.
		  *
		  *	@param	target	Target address to minimize
		  *	@param	host	Host address to minimize relative to
		  *	@param	buffer	Output buffer
		  *	@retrun	void
		  */
		static void minimizeAddress(uint8_t* target, uint8_t* host, uint8_t* buffer);

		/**
		  *	Determines the relationship between the two provided addresses
		  *	relative to host.
		  *
		  *	@param	target	Address to determine relationship of
		  *	@param	host	Address to determine relationship relative to
		  *
		  *	@return	Returns a value as defined in enum LinkType in fct_types.h
		  */
		static uint8_t getRelationship(uint8_t* target, uint8_t* host);

		/**
		  *	Prints the address stored in buffer to standard out
		  *	@param	buffer		Address buffer
		  *	@param	showSize	If true each address chunk will be followed by a
		  *				number in parantheses indicating the number of bits
		  *				used by that chunk, not counting the two bit size field
		  */
		static void printAddress(uint8_t* buffer, bool showSize=true);

		/**
		  *	Prints the bits of the address stored in buffer to standard out
		  *	@param	buffer	Address buffer
		  */
		static void printAddressBits(uint8_t* buffer, bool annotate=false);

		/**
		  *	Prints the bytes of the address stored in buffer to standard out
		  *	@param	buffer	Address buffer
		  */
		static void printAddressBytes(uint8_t* buffer);

		/**
		  *	Compares addr1 and addr2
		  *	@param	addr1
		  *	@param	addr2
		  *	@return	Returns true if the two address are identical
		  */
		static bool compareAddress(uint8_t* addr1, uint8_t* addr2);

};

string addressListToString(vector<raw_address> addresses);
string addressListToString(vector<raw_address*> addresses);
string addressListToString(list<raw_address*> addresses);
string addressListToString(vector<uint8_t*> addresses);

/** COMMON SPECIAL ADDRESSES **/

//! Link local addres 3F.1
static const uint8_t raw_linkLocal[4]={0xfd, 0x10, 0x00, 0x00};

//! Broadcast address 3F.FFF
static const uint8_t raw_broadcast[5]={0xff, 0xff, 0xf0, 0x00, 0x00};

//! Down Link Broadcast address 3F.FFE
static const uint8_t raw_downBroadcast[5]={0xff, 0xff, 0xe0, 0x00, 0x00};

//! Trunk Link Broadcast address 3F.FFD
static const uint8_t raw_trunkBroadcast[5]={0xff, 0xff, 0xd0, 0x00, 0x00};

//! Up Link Broadcast address 3F.FFC
static const uint8_t raw_upBroadcast[5]={0xff, 0xff, 0xc0, 0x00, 0x00};


#define PKT_TYPE_OFFSET ETH_HLEN+offset
#define PKT_LEN_OFFSET PKT_TYPE_OFFSET+2
#define PKT_DST_OFFSET PKT_LEN_OFFSET+2

#define MMT_PKT_TYPE_OFFSET 1
#define MMT_PKT_LEN_OFFSET 3

struct raw_packet{

	raw_packet();
	raw_packet(si_packet& packet);
	raw_packet(raw_packet& other);


	void setPacketType(uint16_t type);
	uint16_t readPacketType() const;

	uint8_t readMMTPacketType() const;

	void setPayloadLength(uint16_t len);
	uint16_t readPayloadLength() const;

	uint16_t readMMTPayloadLength() const;

	/**
	  *	@return	Returns a pointer to the start of the destination address field
	  */
	uint8_t* destinationAddrPtr() const;

	/**
	  *	@return	Returns a pointer to the start of the sender address field
	  */
	uint8_t* senderAddrPtr() const;

	/**
	  *	@return	Returns a pointer to the start of payload data
	  */
	uint8_t* payloadPtr() const;

	/**
	  *	@return	Returns a pointer to the start of MMT payload data
	  */
	uint8_t* mmtPayloadPtr() const;

	uint8_t* dataPtr() const;

	/**
	  *	Increases the size of the payload by len bytes. If buf
	  *	is non-NULL then the first len bytes of buf will be copied into the
	  *	end of the payload section.
	  *
	  *	@param	buf	Buffer to copy from, set to NULL to fill with zeros
	  *	@param	len	Number of bytes to expand payload by
	  *
	  *	@return	Returns false if the packet would become larger than a standard
	  *			ethernet frame.
	  */
	bool appendData(uint8_t* buf, uint16_t len);

	/**
	  *	Removes the last len bytes from the packet
	  *
	  *	@param	len	Number of bytes to remove from packer
	  *	@return	Returns false if payload length would become zero
	  *			after removing len bytes.
	  */
	bool shrinkPayload(uint16_t len);

	/**
	  *	Clears the content of the packet.
	  */
	void clear();

	/**
	  *	Initializes a raw packet with the supplied source and
	  *	destination addresses. Also updates all internal variables
	  *	correctly. If a payload length greater than zero is
	  *	supplied then this will become the packet's payload length
	  *	and all payload bytes will be initialized to zero.
	  *
	  *	@param	dest	Destination address
	  *	@param	src		Source address
	  *	@param	len		Payload length
	  *
	  *	@return	Returns a pointer to the payload section
	  */
	uint8_t* init(uint8_t* dest, uint8_t* src, uint16_t len=0);

	/**
	  *	Removes the first address field from the destination address
	  *	and right shifts the packet header[ETH_HDR, SI_TYPE, LENGTH, DEST_ADDR]
	  *	to use free space.
	  *	@return	Returns false if a parse error occured, buffer is left in an unknown state!
	  */
	bool removeFirstDestAF();
	void printPacket() const;

	raw_packet& operator=(si_packet& other);
	raw_packet& operator=(const raw_packet& other);

	uint8_t data[ETH_FRAME_LEN];
	int16_t length;
	uint16_t offset;
	si_socket* tx_socket;
	si_socket* rx_socket;
};

#endif // RAWUTILS_H
