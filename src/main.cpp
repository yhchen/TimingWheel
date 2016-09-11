#include <windows.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "TimingWheel.h"
#include "Object.h"

Timer::TimingWheel* wheel;
FILE* file = NULL;

class MyObject;

#define reg_call(delay, inter)	do {						\
		MyObject* obj = new MyObject;						\
		unsigned int delayTick = delay;						\
		unsigned int interTick = inter;						\
		interTick = max(1, interTick);						\
		obj->ident = wheel->delayIntervalCall(delayTick,	\
			interTick, obj, &MyObject::MyCallback, NULL);	\
		vecRegedit.push_back(obj->ident);					\
		obj->release();										\
	}while (false)


class MyObject : public Timer::Object
{
public:
	typedef Timer::Object super;

	MyObject() : super() { memset(buf, 0x00, sizeof(buf)); }
	virtual ~MyObject() { }

public:
	void MyCallback(void* param, unsigned int twice);

public:
	char buf[1024];
	const void* ident;
};

void MyObject::MyCallback(void* param, unsigned int twice)
{
// 	sprintf_s(buf, sizeof(buf), "callback jiffies = 0x0%16X\n", wheel->m_ullJiffies);
// 	if (file)
// 	{
// 		fwrite(buf, strlen(buf), 1, file);
// 	}
// 	printf(buf);
	if (twice == 65536)
	{
		wheel->cancelCall(ident);
		//reg_call(this->delayTick % this->uInternal, this->uInternal);
	}
}


int main(int argc, char** argv)
{
	wheel = new Timer::TimingWheel;
	fopen_s(&file, "test_out.txt", "a+");
	srand((unsigned int)time(NULL));
	std::list<const void*> vecRegedit;
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
		if (rand() % 5)
		{
			reg_call(rand() % (1 << 15), rand() % (1 << 16));
		}
		else
		{
			reg_call(rand() % (1 << 8), rand() % (1 << 12));
		}
	}

	for (unsigned int i = 0, j = 0; i < (1 << 31); ++i, ++j)
	{
		wheel->update(1);
		if (j != 0 && j % 65563 == 0)
		{
			if (vecRegedit.size() > 0)
			{
				for (std::list<const void*>::iterator it = vecRegedit.begin(); it != vecRegedit.end(); )
				{
					if ((rand() % 128) == 0)
					{
						wheel->cancelCall(*it);
						vecRegedit.erase(it++);
					}
					else ++it;
				}
			}
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