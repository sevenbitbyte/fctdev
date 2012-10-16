#ifndef SWITCHED_H
#define SWITCHED_H

#include <deque>
#include <string>
#include <list>
#include <inttypes.h>
#include <pthread.h>
#include <linux/if_ether.h>
#include "fct_types.h"
#include "rawutils.h"
#include "pointer.h"
#include "timer.h"

#define USE_DOUBLE_COLON true //! Define this to insert a second ':' between the last address chunk and the extra bytes
#define ETHER_SI 0x8D00 //! Protocol number used in encapsulating ethernet frame
#define BC_DELIMITER "."
#define CHUNK_DELIMITER ":"

#define SI_PACKET_TYPE_SIZE 2
#define SI_PACKET_DATA_LEN_SIZE 2

#define RESERVED_BC 0x3f

//! Reads/writes SI addresses
class si_address {
public:
	//! Creates an address with the default value 3F:0
	si_address(void);

	//! Creates an address by parsing the supplied string
	si_address(std::string addr);

	//! Creates an address by cloning the supplied address
	si_address(const si_address &other);

	//! Creates an address by cloning the supplied raw_address
	//si_address(const raw_address &other);

	static si_address linkLocal(void);
	static si_address broadCast(void);
	static si_address downBroadCast(void);
	static si_address trunkBroadCast(void);
	static si_address upLinkBroadCast(void);

	//! Resets the address to the default value 3F:0
	void clear(void);

	//! Fills in values from the supplied data buffer
	/*!
	 * \param data Data buffer to read from
	 * \param len Size of the supplied data buffer
	 * \return Number of bytes read from the data buffer
	 */
	int readFrom(uint8_t *data, int len);

	//! Writes address out to the supplied data buffer
	/*!
	 * \param data Data buffer to write to
	 * \param len Size of the supplied data buffer
	 * \return Number of bytes written
	 */
	int writeTo(uint8_t *data, int len) const;

	//! Fills in values from the supplied string buffer
	/*!
	 * \param addr An SI address in string format, see toString() for more info
	 * \return EOF if and error occured, otherwise 0 for success
	 * \sa si_address::toString()
	 */
	int parseString(std::string addr);

	//! Compares an SI address against ours and counts the number of continuous matching chunks
	/*!
	 * \param other An SI address to compare against
	 * \return Returns a count of continuous matching chunks
	 */
	int matchingChunks(si_address &other);

	//! Calculates the distance from this SI address to another SI address by hop counting
	//! Assumes there are no trunk connections to shorten path.
	/*!
	  * \param other An SI address to measure distance to
	  * \return Returns the number of hops to other. Zero indicates that other has the same address
	  */
	int hopsTo(si_address &other);

	int byteCount() const;

	//! Generates a string representation of the address
#ifdef USE_DOUBLE_COLON
	/*!
	 * SI addresses are formatted as: BC.ADDR:ADDR ... ADDR:ADDR::EXTRADATA
	 * For example, the loop-back address is 3F:0, broadcast is 3F:FFF and a complex address might be 03:1:3:1::123420081015 (Tier 3, address 1:3:1, extra type 1234, extra value 20081015)
	 */
#else
	/*!
	 * SI addresses are formatted as: BC.ADDR:ADDR ... ADDR:ADDR:EXTRADATA
	 * For example, the loop-back address is 3F:0, broadcast is 3F:FFF and a complex address might be 03:1:3:1:123420081015 (Tier 3, address 1:3:1, extra type 1234, extra value 20081015)
	 *  (Note there is only one colon between the last address chunk and the extra data. The BC value is now used to identify this point in the address string)
	 */
#endif
	std::string toString() const;

	//! Copies one address onto another
	si_address & operator=(const si_address &other);

	//! Calls parseString on the supplied string
	si_address & operator=(const std::string &addr);

	//! Test for equality between two addresses
	bool operator==(const si_address &other) const;

	//! Test for inequality between two addresses
	bool operator!=(const si_address &other);

protected:
	//! Used internally to parse a hex string N nibbles at a time
	static int parseHex(char *string, int len);

public:
	//! The BC value
	uint8_t bc;

	//! The address value chunks
	std::deque<uint16_t> chunks;

	//! The type field of the extra data
	uint16_t extra_type;

	//! The extra data
	std::deque<uint8_t> extra;
};


class si_socket;

//! Reads/writes SI packets
class si_packet {
public:
	//! Creates a default packet
	si_packet(void);

	si_packet(si_address src, si_address dest=si_address::linkLocal(), const uint8_t* payload=NULL, uint16_t len=0);

	//! Creates a packet by cloning the contents of another
	si_packet(const si_packet &other);

	//! Cleans up allocated memory
	~si_packet(void);

	//! Cleans up allocated memory and resets addresses
	void clear(void);



	//! Fills in values from the supplied data buffer
	/*!
	 * \param data Data buffer to read from
	 * \param len Size of the supplied data buffer
	 * \return Number of bytes read from the data buffer
	 */
	int readFrom(uint8_t *data, uint16_t len);

	//! Reads member variables from the internal buffer
	/*!
	  * \return Returns the number of bytes parsed
	  */
	int parseBuffer();

	//! Writes packet out to the supplied data buffer
	/*!
	 * \param data Data buffer to write to
	 * \param len Size of the supplied data buffer
	 * \return Number of bytes written
	 */
	//int writeTo(uint8_t *data, uint16_t len) const;

	//! Writes Ethernet and SI headers into internal buffer
	/*!
	  * \return Returns true if successful
	  */
	bool writeHeaders();

	//! Generates a string representation of the packet
	std::string toString();

	//! Expand the size of the payload by prepending len bytes.
	//! NOTE: Prepended bytes are not initialized.
	/*!
	  * \param len Number of bytes to prepend
	  * \return Returns true if coping was successful, false
	  * if there is not enough space for the payload.
	  */
	bool expandPayload(uint16_t len);

	//! Copies len bytes of data to the packets payload section.
	/*!
	  * \param payload Data to copy to packet's payload
	  * \param len Number of bytes to copy
	  * \return Returns true if coping was successful, false
	  * if there is not enough space for the payload.
	  */
	bool setPayload(const uint8_t* payload, uint16_t len);

	//! Prepends the provided data into the payload section
	/*!
	  * \param data Pointer to data to prepend
	  * \param len Length of data to be prepended
	  * \return Returns false if an error occured, normally this means
	  *	a buffer overrun would take place if the action is carried out
	  */
	bool prependPayload(const uint8_t* data, uint16_t len);

	//! Places the SI headers into the payload section
	bool encapsulateSIHeader();

	bool decapsulate();

	//! Places an SI packet into the payload section
	/*!
	  * \param other SI packet to encapsulate
	  */
	bool encapsulatePacket(si_packet& other);

	//! Pointer to payload, returns NULL if there is no payload
	uint8_t* getPayloadPtr() const;

	//! Number of bytes stored in payload section
	uint16_t payloadLength() const;

	//! Returns a pointer to the front of the packet headers, includes ethernet headers
	uint8_t* getHeaderPtr();

	int siHeaderByteCount();

	//! Total number of bytes in packet, including all headers
	int byteCount() const;

	//! Returns the number of unused bytes in the ethernet frame
	int freeBytes() const;

	void updateBC(si_address* sender=NULL);

	//! Updates the destination address by removing redundant
	//! address fields and setting the dst_addr's BC value
	//! with respect to the sender's address. If sender is NULL
	//! then the packet's src_addr is used instead.
	/*!
	  * \param sender Sending host's address
	  */
	void updateDest(si_address* sender=NULL);

	void restoreDest(si_address* sender=NULL);

	void setDestination(si_address dest);
	void setSource(si_address source);

	const si_address& getDestination() const;
	const si_address& getSource() const;

	//! Copies one packet to another
	si_packet & operator=(const si_packet &other);

	//! Test for equality between two packets
	bool operator==(const si_packet &other);

	//! Test for inequality between two packets
	bool operator!=(const si_packet &other);

	uint16_t type;

	Timer timer;

	vector<string> actionLog;

	si_socket* rx_socket;
	si_socket* tx_socket;

protected:
	friend class si_socket;
	//! Creates a smart pointer to provided data
	/*!
	 * \param data Data buffer to write to
	 * \param len Size of the supplied data buffer
	 */
	void setData(const uint8_t *data, uint16_t len);

	//! Copies the supplied data to internal storage
	/*!
	 * \param data Data buffer to write to
	 * \param len Size of the supplied data buffer
	 */
	void copyData(const uint8_t *data, uint16_t len);

	//! Returns a const pointer to the internal data
	uint8_t *getData() const;

	Pointer<uint8_t> getDataPtr() const;

	//! Returns the internal data length
	int getDataLen() const;

private:
	friend class si_node;
	friend void alarmHandler(int i);
	//! The destination address
	si_address dst_addr;

	//! The source address
	si_address src_addr;

	//! The internal data storage
	Pointer<uint8_t> data_ptr;

	//! Length of the internal data storage
	uint16_t data_len;

	//! Length of payload data
	uint16_t payloadLen;

	//! Length of si headers
	uint16_t siHeaderLen;

	bool dstAddrPacked;
};

//! Reads/writes SI packets on an ethernet interface
/*!
 * While this and the other libraries are very simple and easy to use, it is very important to note that when actually using the
 * si_socket class, you MUST run your program as root! Linux restricts you from directly reading and writing ethernet frames unless
 * you're root.
 */

class raw_packet;

class si_socket {
public:
	//! Constructs an unbound socket
	si_socket(void);

	//! Constructs a socket and binds to the supplied interface name
	si_socket(std::string ifname);

	//! Constructs a socket and binds to the interface of the other socket
	si_socket(const si_socket &other);

	//! Resets the socket to default values and if needed, closes the interface
	void clear(void);

	//! Test for wether or not interface is bound
	bool isOpen(void) const;

	bool isUp(void) const;

	bool setState(bool state) const;

	//! Binds to the supplied interface name
	/*!
	 * \return 0 for success or -1 if an error occured (check errno)
	 */
	int open(std::string ifname, bool ignoreDown=false);

	//! Closes the interface
	void close(void);

	//! Waits for data to read
	bool canRead(int msTimeout=100);

	//! Sends a packet on the interface
	int send(si_packet& packet);

	int send(raw_packet& packet);

	//! Receives a packet on the interface
	/*!
	 * \param packet The packet to send
	 * \param blocking Set to true to do a blocking read, else non-blocking (Default: false)
	 * \return 0 for success or -1 if an error occured (check errno)
	 * This function uses the errno var defined in errno.h in addition to the return value. If set to non-blocking (default) and errno is EAGAIN, then no errors actually occured, but no packets were received this time around. Example:
	 * \code
	 * do {
	 * 	response = sock.recv(pkt, false);
	 * } while (response < 0 && errno == EAGAIN);
	 * \endcode
	 */
	int recv(si_packet& packet, bool blocking=false);

	si_packet* recv(bool blocking=false);

	int recv(raw_packet& packet, bool blocking=false);

	//! Returns a copy of the socket statistics
	socket_stats getStats() const;

	void clearStats(void);

	//! Generates a string representation of the socket
	std::string toString(void) const;

	//! Returns the name of the interface this socket is bound to
	std::string getName(void) const;

	//! The socket handle
	int sock;


private: //TODO: Move this back to above sock

	//! The index of the interface
	int ifindex;

	//! The name of the interface
	std::string ifname;

	//! Ethernet address of the interface
	char src_mac[6];

	//! Mutex to protect sending and recieving
	pthread_mutex_t sendMutex;

	pthread_mutex_t recvMutex;

	socket_stats stats;

};

#endif
