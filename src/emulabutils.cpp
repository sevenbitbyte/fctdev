
#include "emulabutils.h"
#include "iniparser/iniparser.h"
#include "iniparser/dictionary.h"

#include <net/if.h>

#include <string>
#include <list>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string.h>
#include <ctype.h>


using namespace std;

string userConfigPath;

string strtolower(string str){
	for(uint i=0; i < str.size(); i++){
		str[i] = tolower(str[i]);
	}

	return str;
}

bool isEmulab(){
	string ctrl_if = "";

	fstream ctrlStream;
	ctrlStream.open(EMULAB_CONTROLIF, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> ctrl_if;
		ctrlStream.close();

		return (ctrl_if.size() > 0);
	}

	return false;
}

string getControlIFName(){
	string ctrl_if = TESTBED_CONTROLIF;

	fstream ctrlStream;
	ctrlStream.open(EMULAB_CONTROLIF, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> ctrl_if;
		ctrlStream.close();
	}

	return ctrl_if;
}


list<string> getAllIFNames(){
	list<string> if_list;
	string control_if = getControlIFName();
	struct if_nameindex *inames=if_nameindex();
	int i=0;

	while(1){
		if(inames[i].if_index==0 && inames[i].if_name==NULL){
			break;
		}

		if(strlen(inames[i].if_name) > 0){
			if_list.push_back(inames[i].if_name);
		}
		i++;
	}
	if_freenameindex(inames);
	return if_list;
}


list<string> getExperimentIFNames(){
	list<string> if_list;
	string control_if = getControlIFName();

	if(!isEmulab()){
		if_list = getAllIFNames();

		if_list.remove("lo");
		if_list.remove(control_if);

		return if_list;
	}

	fstream ctrlStream;
	ctrlStream.open(EMULAB_IFMAP, fstream::in);

	if(ctrlStream.is_open()){
		while(!ctrlStream.eof()){
			string buf;
			getline(ctrlStream, buf);

			buf = buf.substr(0, buf.find(' '));
			if(buf.length() > 0){
				if_list.push_back(buf);
			}
		}

		ctrlStream.close();
	}

	return if_list;
}


string getRole(){
	string role;
	fstream ctrlStream;
	ctrlStream.open(EMULAB_ROLE, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> role;
		ctrlStream.close();
	}

	return role;
}

string getHostname(){
	char hostname[200];
	gethostname(hostname, 199);

	if( (strlen(hostname)+1) >= 200){
		hostname[200]=0x00;
	}

	return string(hostname);
}

string getNodeName(){
	string nodeName = getHostname();

	uint end = nodeName.find_first_of('.');

	return nodeName.substr(0, end);
}

string getNickName(){
	string nick;
	fstream ctrlStream;
	ctrlStream.open(EMULAB_NICKNAME, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> nick;
		ctrlStream.close();
	}

	return nick;
}

string getCreator(){
	string creator;
	fstream ctrlStream;
	ctrlStream.open(EMULAB_CREATOR, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> creator;
		ctrlStream.close();
	}

	return creator;
}

string getSwapper(){
	string swapper;
	fstream ctrlStream;
	ctrlStream.open(EMULAB_SWAPPER, fstream::in);

	if(ctrlStream.is_open()){
		ctrlStream >> swapper;
		ctrlStream.close();
	}

	return swapper;
}

string getExperimentName(){
	string nick = getNickName();
	if(!nick.empty()){
		uint start = nick.find_first_of('.') + 1;
		uint end = nick.find_last_of('.');

		if(start != end && start != string::npos){
			return nick.substr(start, end-start);
		}

		return nick;
	}

	return string();
}

string getProjectName(){
	string nick = getNickName();
	if(!nick.empty()){
		uint start = nick.find_last_of('.') + 1;

		if(start != string::npos){
			return nick.substr(start);
		}

		return nick;
	}

	return string();
}

string getProjectPath(){
	string path = "/proj/";

	path.append(getProjectName());
	path.append("/");

	return path;
}

string getNSFilePath(){
	string path = getProjectPath();

	path.append("exp/");
	path.append(getExperimentName());
	path.append("/tbdata/nsfile.ns");

	return path;
}

string getConfigPath(){
	if(!userConfigPath.empty()){
		return userConfigPath;
	}

	string path = getProjectPath();

	path.append("groups/config/");
	path.append(getExperimentName());
	path.append("/addresses.ini");

	return path;
}

void setConfigPath(string path){
	userConfigPath = path;
}

string getAssignedAddress(){
	return getAssignedAddress(getNodeName());
}

string getAssignedAddress(string nodeName){
	if(isEmulab()){
		dictionary* dict = iniparser_load(getConfigPath().c_str());

		if(dict != NULL){
			string addr;
			string addrKey = nodeName.append(":address");

			if(iniparser_find_entry(dict, (char*) addrKey.c_str()) == 1){
				addr = iniparser_getstring(dict, addrKey.c_str(), NULL);
			}

			iniparser_freedict(dict);
			return addr;
		}
	}

	return "";
}


int isMMTEnabled(){
	dictionary* dict = iniparser_load(getConfigPath().c_str());

	if(dict != NULL){
		int mmtEA;
		string mmtEAKey = getNodeName().append(":mmt_enabled");

		if(iniparser_find_entry(dict, (char*) mmtEAKey.c_str()) == 1){
			mmtEA = iniparser_getboolean(dict, mmtEAKey.c_str(), -1);
		}

		iniparser_freedict(dict);
		return mmtEA;
	}

	return -1;
}

int getMMTUID(){
	return getMMTUID(getNodeName());
}

int getMMTUID(string nodeName){
	dictionary* dict = iniparser_load(getConfigPath().c_str());

	if(dict != NULL){
		int mmUID;
		string mmtUIDKey = nodeName.append(":mmt_uid");

		if(iniparser_find_entry(dict, (char*) mmtUIDKey.c_str()) == 1){
			mmUID = iniparser_getboolean(dict, mmtUIDKey.c_str(), -1);
		}

		iniparser_freedict(dict);
		return mmUID;
	}

	return -1;
}

