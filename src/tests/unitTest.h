#ifndef UNITTEST_H
#define UNITTEST_H

#include <map>
#include <string>
#include <vector>

using namespace std;

class UnitTest{
	public:
		UnitTest(string name){this->name=name;}
		virtual ~UnitTest();

		virtual bool execute(vector<string> args);
		string getName() const{return name;}
		virtual string getDescription();
		virtual string getHelp(string topic);

	private:
		string name;
};

extern map<string, UnitTest*> testMap;


#define INSERT_TEST(ptr) testMap.insert(make_pair(ptr->getName(), ptr))

#endif // UNITTEST_H
