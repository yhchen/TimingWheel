#ifndef __SG2D_OBJECT_H__
#define __SG2D_OBJECT_H__

#include <atomic>
#include <assert.h>

namespace Timer
{
/**
* 类似于shared_ptr智能指针的实现，引用计数为0时会销毁自身
* retain会增加引用计数，release减少引用计数。创建对象默认
* 引用计数为1。
*/
class Object
{
private:
	/*
	 * 引用计数，最高位表示正在删除标记，为防止对象进入析构函数后调用了retain和release，这样
	 * 会导致多次调用析构函数，为解决该问题，最高位增加删除中标识，其他位用于记录真实引用计数
	 */
	std::atomic_uint_fast32_t m_nRefer;

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
		return m_nRefer._My_val & 0x7FFFFFFF;
// 		return m_nRefer.fetch_and(0x7FFFFFFF, std::memory_order_acquire);
	}
	//增加引用
	inline int retain()
	{
		m_nRefer.fetch_add(1, std::memory_order_release);
		return getRefer();
	}
	//取消引用，当对象引用值为0时将销毁自身
	inline int release()
	{
#ifdef DEBUG
		assert((m_nRefer & 0x7FFFFFFF) > 0);
#endif
		unsigned int ret = m_nRefer.fetch_sub(1, std::memory_order_release);
		if (ret == 0)
		{
			m_nRefer.fetch_or(0x80000000, std::memory_order_release);//设置删除标记
			delete this;
		}
		return getRefer();
	}
};
}

#endif