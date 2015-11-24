#include <windows.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "TimingWheel.h"
#include "Object.h"

SG2D::TimingWheel* wheel;
FILE* file = NULL;

class MyObject;

#define reg_call(delay, inter)	do {						\
		MyObject* obj = new MyObject;						\
		unsigned int delayTick = delay;						\
		unsigned int interTick = inter;						\
		interTick = max(1, interTick);						\
		obj->ident = wheel->delayIntervalCall(delayTick,	\
			interTick, obj, &MyObject::MyCallback, NULL);	\
		obj->uInternal = interTick;							\
		obj->delayTick = wheel->m_ullJiffies + delayTick;	\
		obj->release();										\
	}while (false)


class MyObject : public Object
{
public:
	typedef Object super;

	MyObject() : super() { uInternal = 0; delayTick = 0; memset(buf, 0x00, sizeof(buf)); }
	virtual ~MyObject() { }

public:
	void MyCallback(void* param, unsigned int twice);

public:
	char buf[1024];
	unsigned long long uInternal;
	unsigned long long delayTick;
	const void* ident;
};

void MyObject::MyCallback(void* param, unsigned int twice)
{
	if (twice * uInternal + delayTick != wheel->m_ullJiffies)
	{
		DebugBreak();
	}
// 	sprintf_s(buf, sizeof(buf), "callback jiffies = 0x0%16X\n", wheel->m_ullJiffies);
// 	if (file)
// 	{
// 		fwrite(buf, strlen(buf), 1, file);
// 	}
// 	printf(buf);
	if (twice == 256)
	{
		wheel->cancelCall(ident);
		//reg_call(this->delayTick % this->uInternal, this->uInternal);
	}
}


int main(int argc, char** argv)
{
	wheel = new SG2D::TimingWheel;;
	fopen_s(&file, "test_out.txt", "a+");
	srand(time(NULL));
	reg_call(0, 1);
	reg_call(1, 1);
	reg_call((1<<8), 1);
	reg_call((1<<14), 1);
	reg_call((1<<20), 1);
	reg_call((1<<26), 1);
	reg_call((1<<9) - 1, 1);
	reg_call((1<<15) - 1, 1);
	reg_call((1<<21) - 1, 1);
	reg_call((1<<27) - 1, 1);
	reg_call((1<<9) + 1, 1);
	reg_call((1<<15) + 1, 1);
	reg_call((1<<21) + 1, 1);
	reg_call((1<<27) + 1, 1);
	for (int i = 0; i < 10000; ++i)
	{
// 		if (rand() % 5)
// 		{
// 			reg_call(rand() % (1 << 15), rand() % (1 << 16));
// 		}
// 		else
// 		{
// 			reg_call(rand() % (1 << 8), rand() % (1 << 12));
// 		}
	}

	for (unsigned int i = 0, j = 0; i < (1 << 31); ++i, ++j)
	{
		wheel->update(1);
		if (j != 0 && j % 65563 == 0)
		{
			printf("deal 65536 ticks...\n");
		}
	}

	fflush(file);
	fclose(file);
	file = NULL;
	delete wheel;
	system("pause");
	return 0;
}