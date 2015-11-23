#ifndef __SG2D_OBJECT_H__
#define __SG2D_OBJECT_H__

#include "windows.h"
#include <assert.h>

inline int lock_and(volatile unsigned int &n, int value)
{
	return _InterlockedAnd((long*)&n, value);
}
inline unsigned int lock_inc(volatile unsigned int &n)
{
	return InterlockedIncrement(&n);
}
inline unsigned int lock_or(volatile unsigned int &n, unsigned int value)
{
	return _InterlockedOr((long*)&n, value);
}
inline int lock_dec(volatile unsigned int &n)
{
	return InterlockedDecrement((long*)&n);
}


/**
* 基础对象类，所有SG2D对象的基类。
* 基础类具有引用计数，当对象的引用计数为0时对象会销毁自身。
* 当保存对象指针后通过调用retain增加对象引用计数；删除对象
* 指针引用后使用release减少对象引用计数。
*/
class Object
{
private:
	//引用计数，最高位表示删除标记，当对象正在被销毁时最高位会被置1，其余31位表示实际引用计数。
	//使用引用计数管理对象生命期存在一个需要处理的特殊情形：对象因为引用计数为0而需要被删除，此
	//时在对象的析构函数中可能还会引起此对象被增加和减少引用，当对一个正在销毁的对象增加引用并
	//减少引用时，根据引用计数管理对象生命期原则，release函数会判断递减引用后对象应该销毁，则
	//会再次发起对对象的销毁操作，导致此对象被销毁多次。因此，为避免这种特别的情况发生，必须能够
	//识别出对象是否正在销毁从而正在递减引用计数时进行检测识别并避免多次销毁对象的操作。因此将对
	//象引用计数成员的最高位用于做这个特殊的“正在销毁”的标记。
	unsigned int m_nRefer;

public:
	Object()
	{
		m_nRefer = 1;
	}
	virtual ~Object()
	{

	}
	//获取引用计数
	inline int getRefer()
	{
		return lock_and(m_nRefer, 0x7FFFFFFF);
	}
	//增加引用
	inline int retain()
	{
		return lock_inc(m_nRefer) & 0x7FFFFFFF;
	}
	//取消引用，当对象引用值为0时将销毁自身
	inline int release()
	{
#ifdef _DEBUG
		assert((m_nRefer & 0x7FFFFFFF) > 0);
#endif
		unsigned int ret = lock_dec(m_nRefer);
		if (ret == 0)
		{
			lock_or(m_nRefer, 0x80000000);//设置删除标记
			delete this;
		}
		return ret & 0x7FFFFFFF;
	}
};

#endif