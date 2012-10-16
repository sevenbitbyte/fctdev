
#include "../debug.h"
#include "unitTest.h"
#include <iostream>
#include <sstream>
#include <map>

using namespace std;

// map<string, UnitTest*> testMap = map<string, UnitTest*>();

class AdvPktBuildTests : public UnitTest{
	public:
		AdvPktBuildTests();
		~AdvPktBuildTests();

		bool execute(vector<string> args);
		string getDescription();
		string getHelp(string topic);
};


AdvPktBuildTests::AdvPktBuildTests():UnitTest("AdvPktBuildTests"){
	//
}



AdvPktBuildTests::~AdvPktBuildTests(){

}


bool AdvPktBuildTests::execute(vector<string> args){
	INFO(endl);
	return true;
}

string AdvPktBuildTests::getDescription(){
	return "Tests advertisement packet construction";
}

string AdvPktBuildTests::getHelp(string topic){
	ostringstream sstr;
	sstr<<"No help available for ["<<topic<<"]";

	return sstr.str();
}

//AdvPktBuildTests* ptr=new AdvPktBuildTests();

//INSERT_TEST(ptr);

//testMap.insert(make_pair("test", 0));
