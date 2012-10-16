#include "../debug.h"
#include "../rawutils.h"
#include "../packets.h"
#include "unitTest.h"
#include <iostream>
#include <sstream>
#include <map>


using namespace std;

// map<string, UnitTest*> testMap = map<string, UnitTest*>();

class AdvTests/* : public UnitTest*/{
	public:
		AdvTests();
		~AdvTests();

		bool execute(vector<string> args);
		string getDescription();
		string getHelp(string topic);
};


AdvTests::AdvTests()/*:UnitTest("AdvTests")*/{
	//
}



AdvTests::~AdvTests(){

}


bool AdvTests::execute(vector<string> args){

	/*uint8_t* buf0 = new uint8_t[100];
	memset(buf0, 0xAA, 100);
	raw_packet* pkt = new raw_packet();
	uint8_t* buf1 = new uint8_t[100];
	memset(buf1, 0xAA, 100);*/

	INFO("Allocating "<<200 + sizeof(raw_packet)<<"bytes"<<endl);
	uint8_t* bigBuffer = new uint8_t[200 + sizeof(raw_packet)];
	memset(bigBuffer, 0xAA, 200 + sizeof(raw_packet));

	uint8_t* buf0 = &bigBuffer[0];
	raw_packet* pkt = (raw_packet*)&bigBuffer[100];
	uint8_t* buf1 = &bigBuffer[100+sizeof(raw_packet)];

	pkt->clear();

	cout.unsetf(ios::dec);
	cout.setf(ios::hex);
	INFO("Addresses: &buf0="<<(void*)buf0<<endl);
	INFO("\t &pkt="<<(void*)pkt<<endl);
	INFO("\t &buf1="<<(void*)buf1<<endl);
	cout.unsetf(ios::hex);
	cout.setf(ios::dec);

	si_address siAddr("1.1");
	raw_address rawAddr(siAddr);

	vector<raw_address*> trunkList;
	vector<uint8_t*> addressList;

	bool retVal = buildADVPacket(pkt, (uint8_t*)raw_linkLocal, rawAddr.buffer, -1, addressList, trunkList, 0);

	if(!retVal){
		ERROR("Call to buildADVPacket failed!"<<endl);
		return false;
	}

	for(int i=0; i<100; i++){
		if(buf0[i] != 0xAA){
			ERROR("buf0 Trampled at "<< i<<endl);
			retVal=false;
		}
	}

	for(int i=0; i<100; i++){
		if(buf1[i] != 0xAA){
			ERROR("buf1 Trampled at "<< i<<endl);
			retVal=false;
		}
	}

	delete[] buf0;
	delete pkt;
	delete[] buf1;

	return retVal;
}

string AdvTests::getDescription(){
	return "Tests advertisement packet construction";
}

string AdvTests::getHelp(string topic){
	ostringstream sstr;
	sstr<<"No help available for ["<<topic<<"]";

	return sstr.str();
}


int main(){
	cout<<"Alive"<<endl;
	AdvTests* ptr=new AdvTests();

	vector<string> args;
	if(ptr->execute(args)){
		INFO("No errors"<<endl);
	}
	else{
		ERROR("Errors occured!"<<endl);
	}

	delete ptr;
}

//AdvTests* ptr=new AdvTests();

//extern void* testsMap;

//((map<string, UnitTest*>*)testsMap)->clear();

//testMap.insert(make_pair(ptr->getName(), ptr));

//INSERT_TEST(ptr);

//testMap.insert(make_pair("test", 0));
