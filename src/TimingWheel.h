#pragma once
#include <list>
#include <unordered_map>
#include <assert.h>

#include "Object.h"


namespace SG2D
{
using namespace std;

class TimingWheel
{

public:
	typedef void (Object::*DelayCall) (void* param, unsigned int twice);

	//注册延时调用
	//参数delayMilSec表示延迟事件，单位是毫秒，指定为0表示在下一次TimeCall被更新（下一帧）时进行调用；
	//参数object为调用对象；
	//参数DELAYFUNC为DelayCall类型的回调函数；
	//参数param为回调函数参数；
	//REMARKS：延时调用在执行后会自动被删除，无需手动移除。
	//RETURN：函数返回值表示注册后的调用对象标志值，在取消调用时将使用此值进行对象确认。
	template<typename OBJECTTYPE, typename DELAYCALLFN>
	inline const void* delayCall(unsigned int delayMilSec, OBJECTTYPE* object, DELAYCALLFN func, void* param, bool weakReference = false)
	{
		return delayIntervalCall(delayMilSec, 0, object, func, param, 1, weakReference);
	}

	//注册定时调用
	//参数interval表示调用间隔，单位是毫秒；
	//参数object为调用对象；
	//参数DELAYFUNC为DelayCall类型的回调函数，指定为0则表示每次TimeCall得到更新时（每帧）都将进行调用；
	//参数param为回调函数参数；
	//参数twice为调用次数，指定为0表示无限次数调用，否则表示具体调用的次数。当调用次数达到
	//指定的值后，此定时调用将被移除。
	//RETURN：函数返回值表示注册后的调用对象标志值，在取消调用时将使用此值进行对象确认。
	template<typename OBJECTTYPE, typename INTERVALCALLFN>
	inline const void* intervalCall(unsigned int interval, OBJECTTYPE* object, INTERVALCALLFN func, void* param, unsigned int twice = 0, bool weakReference = false)
	{
		return delayIntervalCall(interval, interval, object, func, param, twice, weakReference);
	}

	//注册延迟定时调用
	//参数delay表示延迟时间，单位是毫秒，指定为0表示在下一次TimeCall被更新（下一帧）时进行调用；
	//参数interval表示调用间隔，单位是毫秒，指定为0则表示每次TimeCall得到更新时（每帧）都将进行调用；
	//参数object为调用对象；
	//参数DELAYFUNC为DelayCall类型的回调函数，指定为0则表示每次TimeCall得到更新时（每帧）都将进行调用；
	//参数param为回调函数参数；
	//参数twice为调用次数，指定为0表示无限次数调用，否则表示具体调用的次数。当调用次数达到指定的值后，此定时调用将被移除。
	//RETURN：函数返回值表示注册后的调用对象标志值，在取消调用时将使用此值进行对象确认。
	template<typename OBJECTTYPE, typename INTERVALCALLFN>
	inline const void* delayIntervalCall(unsigned int delay, unsigned int interval, OBJECTTYPE* object, INTERVALCALLFN func, void* param, unsigned int twice = 0, bool weakReference = false)
	{
#if defined(_DEBUG) || defined(DEBUG)
		assert(dynamic_cast<Object*>(object) == object);
#endif
		union
		{
			INTERVALCALLFN fn;
			DelayCall dc;
		} un;
		un.fn = func;
		return _registerCall(delay, interval, reinterpret_cast<Object*>(object), un.dc, param, twice, weakReference);
	}

public:
	TimingWheel();
	~TimingWheel();

	const void* _registerCall(unsigned int delayMilSec, unsigned int interval, Object* object, DelayCall func, void* param, unsigned int tiwce, bool weakReference);
	void cancelCall(const void* id);

	void update(unsigned int elapse/* tick */);

protected:
	static const int WHEEL_COUNT = 5;		//时间轮数量
	static const int WHEEL_ROOT_BIT = 8;	//第0个时间轮2(param)次幂
	static const int WHEEL_NODE_BIT = 6;	//其他时间轮2(param)次幂

	struct context_cb
	{
		const void* ident;
		Object*	object;
		DelayCall func;
		void* param;
		unsigned long long interval;
		unsigned long long expireTick;
		unsigned int twice;
		unsigned int maxTwice;
		bool weakReference;
		bool removed;
	};

	typedef std::list<context_cb*> 	ContextSlot;
	typedef unordered_map<const void*, context_cb*> ContextMap;

	struct Wheel
	{
		Wheel() :m_nSlotIdx(0), m_Slots(NULL)	{};
		unsigned int	m_nBitCount;			//节点字节数
		unsigned int	m_nNodeCount;			//当前轮节点数量( POW(节点字节数) )
		unsigned int	m_nTickUnitBit;			//每个时间片代表的时间单位(log 2(m_nTickUnit) )
		unsigned int	m_nTickUnit;			//每个时间片代表的时间单位
		unsigned int	m_nSlotMask;			//当前结点“取模”用
		unsigned int	m_nSlotIdx;				//记录当前时间点所指向的slotIdx
		ContextSlot*	m_Slots;
	};

private:
	// 默认使用 new 和 delete 管理context_cb内存，可以拓展为对象池管理提升效率
	virtual context_cb* __AllocContext();
	virtual void __FreeContext(context_cb* cx);

	// 插入context
	inline void __AddContext(context_cb* context);

	// 计算时间轮与槽位下标
	void	__GetTickPos(unsigned int expireTick, unsigned int leftTick, unsigned int &nWheelIdx, unsigned int &nSlotIdx);
	int		__GetTickSlotIdx(unsigned int expireTick, unsigned int leftTick, unsigned int nWheelIdx);

	void	__CascadeTimers(Wheel& wheel);
	void	__Callback(context_cb& context);

public:	// FIXME : remove where
	unsigned long long	m_ullJiffies;			// 流逝的毫秒数
	Wheel*				m_Wheels[WHEEL_COUNT];	// 时间轮
	ContextMap			m_ContextMap;		// 记录回调的会话对应关系
	unsigned int		m_nTotalTickSize;	// 记录当前注册回调的总数量(use for debug)
	unsigned int		m_nNextCallIdent;	// 每个context_cb的唯一Ident标识
};

}