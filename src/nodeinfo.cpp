#include "emulabutils.h"

#include <list>
#include <string>
#include <iostream>

using namespace std;

int main(){
	cout << "Emulab: \t" << isEmulab() << endl;
	cout << "Hostname: \t" << getHostname() << endl;
	cout << "Nodename: \t" << getNodeName() << endl;
	cout << "Assigned Addr:\t" << getAssignedAddress() << endl;

	if(isEmulab()){
		cout << "Role: \t\t" << getRole() << endl;
		cout << "Creator: \t" << getCreator() << endl;
		cout << "Swapper: \t" << getSwapper() << endl;
		cout << "Nickname: \t" << getNickName() << endl;
		cout << "Project: \t" << getProjectName() << endl;
		cout << "Expirement: \t" << getExperimentName() << endl;
		cout << "NS Path: \t" << getNSFilePath() << endl;
		cout << "Address Conf: \t" << getConfigPath() << endl;
	}

	cout << "Control IF: \t" << getControlIFName() << endl;
	cout << "Expirement IFs:\t";

	list<string> interfaces = getExperimentIFNames();

	list<string>::iterator iter = interfaces.begin();
	for(iter; iter != interfaces.end(); iter++){
		cout<<*iter<<"\t";
	}

	cout<<endl<<endl;
}
