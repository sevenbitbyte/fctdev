#ifndef EMULAB_UTILS_H
#define EMULAB_UTILS_H

#include <string>
#include <list>

using namespace std;

#define TESTBED_CONTROLIF "eth0"

#define EMULAB_CONF_DIR		"/var/emulab/boot/"
#define EMULAB_CONTROLIF	EMULAB_CONF_DIR"/controlif"
#define EMULAB_IFMAP		EMULAB_CONF_DIR"/ifmap"
#define EMULAB_ROLE			EMULAB_CONF_DIR"/role"
#define EMULAB_NICKNAME		EMULAB_CONF_DIR"/nickname"
#define EMULAB_CREATOR		EMULAB_CONF_DIR"/creator"
#define EMULAB_SWAPPER		EMULAB_CONF_DIR"/swapper"

bool isEmulab();

/**
  *	The contents of EMULAB_CONTROLIF is return if it exits.
  *	Else, an empty string is returned.
  */
string getControlIFName();

list<string> getAllIFNames();

list<string> getExperimentIFNames();

string getRole();

string getHostname();

string getNodeName();

string getNickName();

string getCreator();

string getSwapper();

string getExperimentName();

string getProjectName();

string getProjectPath();

string getNSFilePath();

string getConfigPath();

void setConfigPath(string path);

string getAssignedAddress();

string getAssignedAddress(string nodeName);

int isMMTEnabled();

int getMMTUID();

int getMMTUID(string nodeName);


#endif	//EMULAB_UTILS_H
