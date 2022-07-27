#include <cstdio>
#include <_Time.h>

int main()
{
	Timer timer;
	timer.begin();
	printf("Fuck!\n");
	timer.end();
	timer.print();
}