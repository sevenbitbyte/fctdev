/**
 * @file si_node.h
 * @short Implements the FCT architecture as well as MMT
 **/

#ifndef SI_NODE_H
#define SI_NODE_H

#include "fct_types.h"
#include "packets.h"
#include "periodic.h"
#include "mmt.h"
#include "switched.h"
#include "mutex.h"
#include "iniparser/iniparser.h"
#include <vector>
#include <map>

using namespace std;


#define MAX_PACKETS 1024*50

//Time in milliseconds
#define MAX_READTIME 1500
#define MAX_LINK_TIME 200


#define ANNOUNCE_PERIOD_SEC		3
#define ANNOUNCE_PERIOD_uSEC	0

#define MMT_ANNOUNCE_PERIOD_SEC		3
#define MMT_ANNOUNCE_PERIOD_uSEC	0

#define HALF_ANNOUNCE_PERIOD_SEC	(ANNOUNCE_PERIOD_SEC>>1)
#define HALF_ANNOUNCE_PERIOD_uSEC	((((ANNOUNCE_PERIOD_SEC*uS_IN_SEC)>>1) - HALF_ANNOUNCE_PERIOD_SEC*uS_IN_SEC) + (ANNOUNCE_PERIOD_uSEC>>1))

//#define LINK_WATCHDOG_TMR_SEC	(ANNOUNCE_PERIOD_SEC + HALF_ANNOUNCE_PERIOD_SEC)
//#define LINK_WATCHDOG_TMR_uSEC	(ANNOUNCE_PERIOD_uSEC + HALF_ANNOUNCE_PERIOD_uSEC)

#define LINK_MISSED_PKT_DELETE_THRESH	(3)
#define LINK_MISSED_PKT_ALT_PATH_THRESH	(1)

#define MAX_LINK_AGE_SEC    (ANNOUNCE_PERIOD_SEC*3)
#define MAX_LINK_AGE_uSEC   0

#define INDIRECT_LINK	false
#define DIRECT_LINK		true

#define MIN_REPORT_INTERVAL_MS 5000


#define ALLOC_ADDRESS_TIMEOUT_SEC	(ANNOUNCE_PERIOD_SEC << 1)
#define ALLOC_ADDRESS_TIMEOUT_uSEC	(ANNOUNCE_PERIOD_uSEC << 1)

#define MAX_VID_LENGTH			10
#define MAX_CLUSTER_SIZE		10
#define MAX_CLUSTER_HEAD_VIDS	2
#define MAX_GENERAL_NODE_VIDS	3

struct link_info{
	uint8_t type;
	bool mmt_enabled;
	bool hasFailed;
	bool removable;
	bool direct;
	bool newNode;
	struct timeval last_seen;
	struct timeval last_announce;
	//si_address address;
	uint16_t lastSequence;
	uint8_t missed_announcements;
	raw_address address;
	raw_address minAddress;
	vector<raw_address> secondaryAddresses;
	vector<raw_address> trunkList;
	si_socket* socket;
	string hostname;
	uint32_t tx_count;
	uint32_t rx_count;

	periodic* watchdog;

	void printInfo();
	string getName();

	link_info();
	~link_info();
};


typedef struct routing_statistics
{
    bool initial_convergance;
    unsigned int table_size;
    bool table_changed;
    int routing_table_counter;
    struct timeval converganceTime;
    bool convergance;
    struct timeval lastconvergence;
    unsigned int convergance_counter;

}stats;


/*struct si_graph{
	map<uint16_t, si_graphNode*> roots;
	uint8_t tierLevel;
};

struct si_graphNode{
	address_chunk chunk;
	si_graphNode* parent;
	map<uint16_t, si_graphNode*> children;
	map<uint16_t, si_graphNode*> siblings;
	mmt_info* info;
};*/

/*struct routing_stats{
	Timer t;
	uint32_t packets;
	uint32_t byteCount;
	uint32_t upCount;
	uint32_t downCount;
	uint32_t trunkCount;
	uint32_t dropCount;
	uint32_t consumeCount;
	uint32_t processCount;

	routing_stats(){
		t.reset();
		packets=0;
		byteCount=0;
		upCount=0;
		downCount=0;
		trunkCount=0;
		dropCount=0;
		consumeCount=0;
		processCount=0;
	}
};*/

class si_node : public periodic_t{
	public:
		si_node(si_address address, bool announceEA=true);
		si_node(string configPath);
		~si_node();

		void setAnnounceEnable(bool announceEA=true);
		bool isAnnounceEnabled() const;

		void setLocalAddress(si_address addr);
		si_address getLocalAddress() const;

		void setMMTEnable(bool state);
		bool getMMTEnabled();

		void setMMTUid(uint16_t uid);
		uint16_t getMMTUid() const;

		void addVid(vid_t newVid);
		void setVidList(vector<vid_t> vidList);
		vector<vid_t> getVidList() const;
		void clearVidList();

		uint8_t getMMTNodeType() const;

		const mmt_info* getMMTInfo() const;

		bool isOrphaned() const;

		void setOrphanState(bool state);

		/**
		  *	Checks if the node already has the interface named by ifname
		  *
		  *	@param	ifname	Name of the interface to check for
		  *	@return	Returns true if the interface has already been added
		  */
		bool hasInterface(string ifname);

		/**
		  *	Adds all non-control interfaces
		  *
		  *	@return Returns true if all interfaces were added successfully
		  */
		bool addAllInterfaces(list<string> exclude = list<string>());

		/**
		  *	Adds the interface specified by ifname
		  *
		  *	@param	ifname	    Interface to operate on
		  *	@return	Returns true if interface was added successfully
		  */
		bool addInterface(string ifname);

		/**
		  *	Removes the interface specified by ifname
		  *
		  *	@param	ifname	Interface to add
		  *	@return	Returns true if interface could be added
		  */
		bool removeInterface(string ifname);

		/**
		  *	Returns a pointer to an already added interface
		  *
		  *	@param	ifname	Interface to retrieve
		  *	@return	Returns a pointer to the associated si_socket instance
		  *		or NULL if the interface has not already been added
		  */
		si_socket* getInterface(string ifname);

		/**
		  *	Returns a vector of pointers to the link_info structs associated
		  * with a specified interface
		  *
		  *	@param	ifname	Interface name to lookup
		  *	@return	Returns a vector of pointers to the associated link_info
		  *			instances or an empty vector if no neighbors have been
		  *			detected on the specified interface.
		  */
		vector<link_info*> getLinkInfo(string ifname);

		/**
		  *	Finds the link information for a node with the given address on
		  *	the specified socket.
		  *
		  *	@param	socket	Socket that link is expected on.
		  *	@param	address	Address of desired link
		  *
		  *	@return Returns a pointer to a valid link_info struct or NULL if the
		  *			no such link exists.
		  */
		link_info* getLinkInfo(si_socket* socket, raw_address* address) const;


		vector<link_info*> getAllLinks() const;

		vector<link_info*> getLinksByType(uint8_t type) const;

		inline bool updateLink(raw_address address, link_info* link);

		/**
		  *	Transmits a packet on all interfaces
		  *
		  *	@param	packet	Pointer to the packet to transmit
		  */
		void broadcast(raw_packet* packet=NULL);

		/**
		  *	Blocks until an announce packet has been reiceived from all
		  *	configured links.
		  *
		  *	@param	msTimeout	Maximum time to spend listending for neighbors
		  *	@return Returns true if all neighbors have been seen before the
		  *		timer expires
		  */
		uint32_t neighborWait(int msTimeout=4000);

		/**
		  *	Routes and transmits the provided packet
		  */
		void injectPacket(si_packet* packet);

		/**
		  *	Checks to determine what sockets can be read immediatly without blocking
		  *	@param	msTimeout	Max time to wait for packets
		  *	@return Returns a list of all sockets that are ready for reading
		  */
		vector<si_socket*> getReadySockets(int msTimeout=500);

		/**
		  *	Reads packets from the links contained in readFrom and
		  *	returns the packets in a vector
		  *	@param	readFrom	links to read packets from
		  *	@param	blocking	whether to block when reading
		  *	@return	Returns a vector of the packets that read in
		  */
		vector<si_packet*> readPackets(vector<link_info*> readFrom, bool blocking=false);

		/**
		  *	Reads and routes packets read from the sockets passed in readFrom
		  *	@param	readFrom	Sockets to read packets from
		  *	@param	blocking	block when reading, defaults to false
		  */
		void processPackets(vector<si_socket*> readFrom, bool blocking=false);

		//!	Routes the si_packet by sending, consuming, or processing
		bool routePacket(raw_packet* packet=NULL);

		/**
		  *	Transmits the specified packet using mmt to the appropriate internal interface
		  *	packet must already be an encapsulated MMT packet
		  *
		  *	@param	packet	MMT encapsulated packet to transmit via MMT
		  *	@return	Returns true if a valid MMT route was found and packet was transmitted successfully
		  */
		bool routeMMT(raw_packet* packet);

		/**
		  *	Transmits the specified packet using mmt to the appropriate internal interface
		  *	packet must be an unencapsulated SI packet
		  *
		  *	@param	packet	SI packet to encapsulate and transmit via MMT
		  *	@return	Returns true if a valid MMT route was found and packet was transmitted successfully
		  */
		bool mmtForward(raw_packet* packet);

		/**
		  *	Starts running the FCT routing algorithm in a dedicated thread
		  *	@return	Returns true if the thread was started successfully
		  */
		bool start();

		/**
		  *	Stops the routing thread if it is running
		  *
		  *	@param	blocking    If true waits for thread to stop before returning
		  *	@param	msTimeout   Maximum time to wait for routing thread to exit before returning
		  *			    if msTimeout<=0 then function does not timeout
		  *	@return	Returns true if routing thread exited before timer expired
		  */
		bool stop(bool blocking=true, int msTimeout=1500);

		/**
		  *	Checks if routing thread is running
		  *
		  *	@return	Returns true if routing thread is running
		  */
		bool isRunning();

		/**
		  *	Routing thread entry point, this funtion should not be called directly.
		  *	Runs the routing loop at high priority until thread is terminated
		  *
		  *	@param	target	si_node instance to use cast to a void*
		  *	@return	Exit value of routing thread
		  */
		static int run(void* target);

		/**
		  *	Executes one loop of waiting for a ready link, reading packets and
		  *	routing the packets that could be read without blocking. Updates
		  *	routing statistics
		  */
		void runOnce();

		/**
		  *	If we actually delivered packets to the application layer
		  *	this is where it would take place. For now we just print
		  *	debug info then delete the packet.
		  *	@param	packet	Packet to delete
		  */
		void consumePacket(raw_packet* packet);

		/**
		  *	Set a function to be called when a packet addressed to this
		  *	node is recieved. Callback is responsible for deleting the
		  *	buffer passed to it.
		  */
		void setConsumeCallback(void (*funcPtr)(raw_packet*));

		void setNewLinkCallback(void (*funcPtr)(link_info*));

		void setDeadLinkCallback(void (*funcPtr)(link_info*));

		void setAltLinkCallback(void (*funcPtr)(link_info*,link_info*));

		void setAddressChangeCallback(void (*funcPtr)(si_address,si_address));

		void setOrphanedCallback(void (*funcPtr)(bool));

		void setMissedHelloCallback(void (*funcPtr)(link_info*));

		/**
		  *	Display the Entries in The upMap, downMap and trunk table
		  *
		  */
		void displayUpRoutingTable();
		void displayDownRoutingTable();
		void displayTrunkRoutingTable();
		void displayRoutingTable(string Table_name, map<uint16_t, link_info*> *Map);

		/**
		  *	To calculate time required in milliseconds to update/Insert record into
		  */
		float calcaulateTime(timeval a, timeval b);
		//void checkInitialConvergence();

		bool reportInitialConvergance(struct routing_statistics *table);
		void InitializeRoutingStats(struct routing_statistics* table);
		void checkInitialConvergence(string table_name, struct routing_statistics* table, map<uint16_t, link_info*> *Map);

		void reportPacket(si_packet* packet);

		void reportRouteRate();

		bool reportInitialConvergance();

		void setInitialConvergance(bool value);

	protected:

		//! Safe receive
		inline int recvPacket(raw_packet& pkt, si_socket& socket, bool blocking=false);

		//! Safe send
		inline int sendPacket(raw_packet& pkt, si_socket& socket);

		//! Periodic service routines
		void announce(void* arg, periodic* p);
		void checkLinkStatus(void* arg, periodic* p);
		void announceCleanup(void* arg, periodic* p);
		void checkAllocatedAddress(void* arg, periodic* p);

		void announceMMT(void* arg, periodic* p);
		void announceMMTCleanup(void* arg, periodic* p);

		void rebuildAnnouncement();
		void rebuildMMTAnnouncement();

		void removeDeadLinks();

		/**
		  *	Removes all references to the specified link in data structures
		  *	used for packet routing.
		  *
		  *	@param	link	Link pointer to remove from routing
		  */
		void removeMapEntries(link_info* link);

		/**
		  *	First calls removeMapEntry to clean up the routing data structures
		  *	then removes all other references to link and deletes the instance
		  */
		void deleteLink(link_info* link);

		inline void processConfPacket(raw_packet* packet, uint8_t minor, uint8_t* payloadPtr, uint8_t* sourceAddrPtr);

		void printLinkState();
		void printRoutingMaps();
		void printRoutingMap(map<uint16_t, link_info*> table);

		bool orphaned;
		vector<si_packet*> userPackets;
		vector<link_info*> links;
		multimap<si_socket*, link_info*> socketMap;
		map<raw_address, link_info*> addrLinkMap;
		vector<si_socket*> sockets;

		//! Map from first address field to link_info*
		map<uint16_t, link_info*> upMap;

		//! Map from last address field to link_info*
		map<uint16_t,  link_info*> downMap;

		//! Map from last address field to link_info*
		map<uint16_t, link_info*> trunkMap;

		//! List of all link_info* which are internal
		map<si_socket*, link_info*> internalLinks;


		//MMT routing structures
		//! Keyed on first SI AF
		//map<uint16_t, mmt_info*> mmtUpMap;
		//! Keyed on last SI AF
		//map<uint16_t, mmt_info*> mmtDownMap;
		//! Keyed on last SI AF
		//map<uint16_t, mmt_info*> mmtTrunkMap;

		mmt_tables mmt;
		mmt_info mmtInfo;
		//vid_tree vidTree;
		//map<uint16_t, mmt_info*> mmtUidMap;

		raw_packet routeBuf;

		//Announcement variables
		bool announceEnabled;
		raw_packet announcePkt;
		uint16_t announceSeq;
		uint8_t* announcePayload;
		uint32_t announcedBytes;
		map<link_info*, raw_packet*> announceTargets;

		//MMT Announcement variables
		bool announceMMTEnabled;
		raw_packet announceMMTPkt;
		uint8_t* announceMMTPayload;
		uint32_t announcedMMTBytes;

		Mutex linkMutex;
		Mutex socketMutex;
		Mutex packetMutex;
		Mutex socketMapMutex;

		//Local address information
		si_address local_address;
		raw_address rawLocalAddress;
		uint8_t* localAddressBuffer;		//TODO: Phasing out, use rawLocalAddress instead!
		int32_t localAddressSize;
		uint8_t local_bc;
		string local_hostname;
		bool mmt_enabled;

		//! Secondary address list
		list<raw_address*> addressList;

		stats upMapTable;
		stats downMapTable;
		stats trunkMapTable;

		//! Routing thread stop state
		bool stopped;
		//! Routing thread termination request flag
		bool terminated;
		//! Posix thread ID
		int _tid;
		//! Routing thread's stack space
		uint8_t* _stack;
		//! Timer for measuring time spent in runloop
		Timer runloop;


		//To keep track whether the routing table is being displayed for the first time.
		// int counter_routing;
		// bool table_changed;
		// unsigned int table_size;
		// int routing_table_counter;
		// bool initial_convergance;
		// struct timeval converganceTime;
		struct timeval start_Routing_Time;

		//! Variables to check Initial node convergence
		struct timeval temp_upMap_table;
		struct timeval temp_downMap_table;
		struct timeval temp_trunkMap_table;


		int avgTime;
		float avgRate;
		uint32_t packetCount;
		uint32_t byteCount;

		uint32_t totalPackets;
		uint64_t totalBytes;
		uint32_t droppedPackets;
		uint64_t droppedBytes;

		//! User space function to call when packets are consumed
		void (*consumeCallback)(raw_packet*);

		//! User space function to call when new links are discovered
		void (*newLinkCallback)(link_info*);

		//! User space function to call when links are removed
		void (*deadLinkCallback)(link_info*);

		//! User space function to call when link is changed to use an alternate path
		void (*usingAltPathCallback)(link_info*, link_info*);

		//! User space function to call when this node's address changes
		void (*addressChangeCallback)(si_address, si_address);

		//! User space function to call when this node's orphan status changes
		void (*orphanedCallback)(bool);

		//! User space function to call when a missed annoucement has been detected
		void (*missedHelloCallback)(link_info*);

		void (*clusterHeadCallback)(mmt_info*);
};

#endif
