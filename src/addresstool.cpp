#include <iostream>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>


#include "debug.h"
#include "switched.h"
#include "rawutils.h"
#include "fct_types.h"

#define _EMULAB false



using namespace std;

string linkTypeToString(uint8_t type);
uint8_t determineLinkType(uint8_t* target, uint8_t* raw_LocalAddress);

int main(int argc, char** argv){
	if(argc < 2){
				cout<<"Usage: "<<argv[0]<<" [si_address] <si_address>"<<endl;
		exit(1);
	}

	uint8_t addressBuf0[200];
	uint8_t addressBuf1[200];
	bzero(addressBuf0, 200);
	bzero(addressBuf1, 200);


	si_address addr1(argv[1]);
	raw_address rawAddr1(addr1);
	cout<<"Read address: "<<rawAddr1.toString()<<endl;

	addr1.writeTo(addressBuf1, 200);



	cout<<"Has "<<raw_address::chunkCount(addressBuf1)<<" chunks and "<<raw_address::byteCount(addressBuf1)<<" bytes"<<endl;
	raw_address::printAddressBytes(addressBuf1);
	cout<<endl;
	raw_address::printAddressBits(addressBuf1);
	cout<<endl;



	if(argc >= 3){
		uint8_t addressBuf2[200];
		uint8_t minAddressBuf[200];
		memset(addressBuf2, 0x00, 200);
		memset(minAddressBuf, 0x00, 200);

		si_address addr2(argv[2]);
		cout<<endl<<"Read address2: "<<addr2.toString()<<endl;

		addr2.writeTo(addressBuf2, 200);

		cout<<"Has "<<raw_address::chunkCount(addressBuf2)<<" chunks and "<<raw_address::byteCount(addressBuf2)<<" bytes"<<endl;
		raw_address::printAddressBytes(addressBuf2);
		cout<<endl;
		raw_address::printAddressBits(addressBuf2);
		cout<<endl<<endl;


		cout<<"Minimum Address From [";
		raw_address::printAddress(addressBuf1, true);
		cout<<"] To [";
		raw_address::printAddress(addressBuf2, true);
		cout<<"]"<<endl;

		raw_address::minimizeAddress(addressBuf2, addressBuf1, minAddressBuf);
		raw_address::printAddress(minAddressBuf, false);
		cout<<endl;
		cout<<"Has "<<raw_address::chunkCount(minAddressBuf)<<" chunks and "<<raw_address::byteCount(minAddressBuf)<<" bytes"<<endl;
		raw_address::printAddressBytes(minAddressBuf);
		cout<<endl;

		raw_address::printAddressBits(minAddressBuf);
		cout<<endl;


		uint8_t linkType = determineLinkType(addressBuf2, addressBuf1);

		cout<<"LinkType: "<<linkTypeToString(linkType)<<(int)linkType<<endl;
	}
}

string linkTypeToString(uint8_t type){
	ostringstream sstr;

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

uint8_t determineLinkType(uint8_t* target, uint8_t* raw_LocalAddress){
	if(raw_address::readBC(target) == RESERVED_BC){
		return (uint8_t) INVALID;
	}

	uint8_t minAddr[100];
	int tierDelta=0;
	uint16_t chunks=0;
	uint8_t minBC=0;
	uint8_t local_bc = raw_address::readBC(raw_LocalAddress);

	memset(minAddr, 0x00, 100);

	raw_address::minimizeAddress(target, raw_LocalAddress, minAddr);

	minBC = raw_address::readBC(minAddr);
	tierDelta = minBC - local_bc;
	chunks = raw_address::chunkCount(minAddr);


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
