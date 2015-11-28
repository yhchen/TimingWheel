#pragma once
#include <unordered_map>
#include <assert.h>
#ifdef _WIN32
#	include "queue.h"
#else
#	include <sys/queue.h>
#endif


namespace SG2D
{
using namespace std;

class Object;

/************************************************************************
 * 时间轮方式实现的定时器
 * 整理架构 :
 *   m_Wheels[0]是workwheel，每个slot对应一个tick(ms)，当时间到了可以不需要
 *   判定直接将该slot中所有callback处理掉。算法效率O(1)
 *   m_Wheels[1 ~ WHEEL_COUNT-1]是assistwheel，每个slot的slotUnitTick都是
 *   上一层slotCount * slotUnitTick。用于管理更大的时间基数。当下层的wheel
 *   转完一圈后，将调用__cascadeTimers移动m_Slots[m_uSlotIdx]内定时器至下层
 * 
 * 
 * example:
 *   int main(int argc, char** argv) {
 *       TimingWheel wheel;
 *       bool terminal = false;
 *       const int frameTick = 50;
 *       int beginTick = getTickCount();
 *       while (!terminal) {
 *           wheel.update(frameTick);
 *           int leftTick = frameTick - (GetTickCount() - beginTick);
 *           if (leftTick > 0)
 *           {
 *               Sleep(leftTick);
 *           }
 *           beginTick += frameTick;
 *       }
 *       return 0;
 *   }
 ************************************************************************/
class TimingWheel
{
public:
	// callback函数格式
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
		return delayIntervalCall(delayMilSec, 0xFFFF, object, func, param, 1, weakReference);
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

	const void* _registerCall(unsigned int delayMilSec, unsigned int interval, Object* object, DelayCall func, void* param, unsigned int twice, bool weakReference);
	void	cancelCall(const void* ident);	// 取消ident callback
	void	update(unsigned int elapse/* tick */);	//通知流逝tickCount(ms) 一般在主循环中调用
	void	removeAllCall();	// 移除所有callback

protected:
	static const int WHEEL_COUNT = 5;		//时间轮数量([0]work + [n-1]assist)
	static const int WORK_WHEEL_BIT = 8;	//工作轮2(param)次幂
	static const int ASSIST_WHEEL_BIT = 6;	//辅助轮2(param)次幂

	struct Wheel;
	struct context_cb;
	TAILQ_HEAD(ContextSlot, context_cb);
	typedef unordered_map<const void*, context_cb*> ContextMap;

private:
	// 默认使用 new 和 delete 管理context_cb内存，可以拓展为对象池管理提升效率
	virtual context_cb* __allocContext();
	virtual void __freeContext(context_cb* cx);

	// 插入context
	inline void __addContext(context_cb* context);
	inline void __moveContext(context_cb* context) { __addContext(context); }
	inline void __repushContext(context_cb* context) { __addContext(context); }
	// 计算时间轮与槽位下标
	inline void	__getTickPos(unsigned long long expireTick, unsigned long long leftTick, unsigned int &nWheelIdx, unsigned int &nSlotIdx);
	inline int	__getTickSlotIdx(unsigned long long expireTick, unsigned long long leftTick, unsigned int nWheelIdx);
	// 移动assist wheel中context_cb
	void		__cascadeTimers(Wheel& wheel);

private:
	Wheel*				m_Wheels[WHEEL_COUNT];	// 时间轮
	const void*			m_pCurrCallIdent;		// 当前正在调用中的ident
	size_t				m_uNextCallIdent;		// 每个context_cb的唯一Ident标识
// 	unsigned int		m_uFrameTickUnit;		// 每一帧的毫秒数
// 	unsigned int		m_uCachedTicks;			// update中传入的elapse可能非m_uFrameTickUnit整数倍，未处理的时间缓存在此
	unsigned long long	m_ullJiffies;			// 流逝的时间片单位
	ContextMap			m_ContextMap;			// 记录回调的会话对应关系
};

}