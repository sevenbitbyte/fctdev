#ifndef MMT_H
#define MMT_H

#include "fct_types.h"
#include "rawutils.h"

#include <map>
#include <vector>

using namespace std;

class link_info;

#define MAX_VID_LENGTH 10
#define MAX_MMT_CHILDREN 50
#define MAX_VID_COUNT 2

/*
 *	NOTE: Talk to Yoshi
 *	This seems to imply that MMT can't be used
 *	where only Down links are present.
 */
#define IS_CLUSER_HEAD(x) (x&(MMT_Trunk|MMT_Up))

enum MMT_States{
	MMT_Idle=0x0,
	Join_Requested,
	Join_Accepted,
	Join_Rejected,
};

class vid_treeNode;

struct mmt_info {
	mmt_info();
	mmt_info(uint16_t u);
	mmt_info(uint8_t type, uint16_t id, uint8_t* vidListPtr, link_info* link=NULL);

	void printInfo();

	uint16_t uid;
	uint8_t mmtType;
	link_info* link;
	vector<vid_t> vidList;
	vector<vid_treeNode*> vids;
	vector<raw_address> neighbors;
	multimap<uint16_t, vid_treeNode*> clusterHeads;
};

struct vid_treeNode{
	~vid_treeNode();

	vid_t getVid();

	uint8_t digit;
	uint8_t depth;
	uint8_t state;	//! Indicates our current association
	vid_treeNode* parent;
	map<uint8_t, vid_treeNode*> children;

	mmt_info* info;
};

class mmt_tables {
	public:
		~mmt_tables();

		mmt_info* getInfoByUid(uint16_t id);
		mmt_info* getInfoByVid(vid_t vid);
		mmt_info* getInfoByRawVid(uint8_t* bufferPtr);
		mmt_info* getInfoByRawVidList(uint8_t* listPtr);

		mmt_info* getNearestNodeByVid(vid_t vid);
		mmt_info* getNearestNodeByRawVid(uint8_t* bufferPtr);

		bool insertNode(mmt_info* info);
		bool updateNodeVids(mmt_info* info);
		bool updateNodeVids(uint16_t id, uint8_t* listPtr);

		vid_treeNode* createVidNode(vid_t vid);
		vid_treeNode* createVidNode(uint8_t* bufferPtr);

	protected:
		map<uint16_t, mmt_info*> uidMap;
		map<uint8_t, vid_treeNode*> vidRoots;
};


/*struct vid_tree{
	~vid_tree();
*/
	/**
	  *	Constructs an mmt_info struct and inserts it into the tree
	  *
	  *	@param	advertBuffer	Pointer to a packet to parse
	  *	@param	rxLink			Link that node was detected on
	  *	@return	Returns the constructed mmt_info struct
	  *
	  */
	//mmt_info* construct(uint8_t* advertBuffer, link_info* rxLink);

//	map<uint8_t, vid_treeNode*> roots;

	//bool insert(vid_t vid, link_info* link);
	//bool insert(uint8_t* vidBuffer, uint8_t size, mmt_info* info);
/*	vid_treeNode* create(uint8_t* vidBuffer, uint8_t size);

	void remove(vid_t vid);
	void removeNode(vid_treeNode* node);

	vid_treeNode* nearestLink(vid_t vid);
	vid_treeNode* nearestLink(uint8_t* vidBuffer, uint8_t size);

	vid_treeNode* getNode(vid_t vid);
	//vid_treeNode* getNode(uint8_t* vidBuffer, uint8_t size);
	vid_treeNode* getNode(uint8_t* vidListPtr);
};*/


/*
#include "rawutils.h"
#include "si_node.h"
#include "switched.h"

#include <map>
#include <list>
#include <vector>


using namespace std;
*/


/*class vid_t : public std::vector<uint8_t> {
	public:
		vid_t(): std::list<uint8_t>(), parent_uid(0) {}

		vid_t(std::_List_iterator<uint8_t> a, std::_List_iterator<uint8_t> b): std::list<uint8_t>(a,b), parent_uid(0) {}

		~vid_t() {}

		uint8_t parent_uid;
};*/

/*struct mmt_field{
	length;
}*/

/*struct mmt_info{
	uint16_t uid;
	uint8_t mmtType;
	vector<vid_t> vidList;
	si_raw_address neighbor;
};

struct vid_tree{
	map<uint8_t, vid_treeNode*> roots;

	void insert(vid_t vid, link_info* link);
	void remove(vid_t vid);
	vid_treeNode* getNode(vid_t vid);
};

struct vid_treeNode{
	uint8_t digit;
	vid_treeNode* parent;
	map<uint8_t, vid_treeNode*> children;

	link_info* link;
	mmt_info* info;
};*/

/*
class mmt_node : public si_node{
	public:
		mmt_node(si_address addr, uint16_t id);
		mmt_node(si_node* node, uint16_t id);
		~mmt_node();

		void enableMMT(si_socket* interface=NULL);
		void disableMMT(si_socket* interface=NULL);
		bool isMMTEnabled(si_socket* interface=NULL);

		void encapsulatePacket(si_socket* packet);

		//Overridden functions
		void consumePacket(raw_packet* packet);
		bool routePacket(raw_packet* packet=NULL);

	private:

		uint16_t uid;
		//uint8_t type;

		vector<vid_t> upNodes;			//Don't need si_address
		map<vid_t, si_address> downNodes;
		map<vid_t, si_address> trunkNodes;

		vector<vid_t> vidList;

		//map<vid_t, link_info*> vidLinkMap;

		vid_tree vidTree;
		map<uint16_t, link_info*> uidMap;
		multimap<type, mmt_info*>

		link_info* getBestLink(vid_t vid);*/
//};

#endif
