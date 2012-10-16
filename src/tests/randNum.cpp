#include <stdio.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

int main(){

	int limit = 200;

	srand(1);

	for(int i=0; i<limit; i++){
		int randVal = rand();

		unsigned int value16 = randVal % 0xffff;
		unsigned int mask16 = randVal & 0xffff;

		cout<<"Rand = "<<randVal<<"\t uint16_t="<<value16<<"\t mask16="<<mask16<<endl;
	}
}
