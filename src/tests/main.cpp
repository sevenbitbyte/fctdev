#include "../debug.h"
#include "unitTest.h"

#include <iostream>
#include <string>
#include <map>

using namespace std;

map<string, UnitTest*> testMap = map<string, UnitTest*>();

int main(int argc, char** argv){
	INFO(VERSION_STRING<<endl);

	cout<<endl<<endl;

	map<string, UnitTest*>::iterator iter = testMap.begin();


}
