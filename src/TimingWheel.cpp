#include "TimingWheel.h"
#include "Object.h"

namespace SG2D{

#ifndef max
#	define max(a, b)		(((a) > (b)) ? (a) : (b))
#endif // !max

#define OFFSET(className,memberName)			((char *)&((className *)0)->memberName - (char *)0)
#define OFFSETVAL(className, memberName, memberType, target)			\
			*((memberType*)((char*)target + OFFSET(className, memberName)))

///--------------------------------
TimingWheel::TimingWheel() :m_ullJiffies(0)
{
	m_nNextCallIdent = 1;
	for (int wIdx = 0; wIdx < WHEEL_COUNT; ++wIdx)
	{
		unsigned int uBitCount = ((wIdx == 0) ? WHEEL_ROOT_BIT : WHEEL_NODE_BIT);
		unsigned int uSlotCount = 1 << uBitCount;
		unsigned int uTickUnitBit = ((wIdx == 0) ? 0 : (WHEEL_ROOT_BIT + WHEEL_NODE_BIT * (wIdx-1)));
		unsigned int uSlotUnitTick = ((wIdx == 0) ? 1 : m_Wheels[wIdx-1]->uSlotUnitTick << m_Wheels[wIdx-1]->uBitCount);
		unsigned int uSlotMask = uSlotCount - 1;
		m_Wheels[wIdx] = new Wheel(uBitCount, uSlotCount, uTickUnitBit, uSlotUnitTick, uSlotMask);
		m_Wheels[wIdx]->m_Slots = new ContextSlot[m_Wheels[wIdx]->uSlotCount];
		for (int sIdx = 0; sIdx < m_Wheels[wIdx]->uSlotCount; ++sIdx)
		{
			// FIXME : 初始化节点
		}
	}
}

TimingWheel::~TimingWheel()
{
	for (int nWheelIdx = 0; nWheelIdx < WHEEL_COUNT; ++nWheelIdx)
	{
		Wheel* pWheel = m_Wheels[nWheelIdx];
		for (unsigned int nNodeIdx = 0; nNodeIdx < pWheel->uSlotCount; ++nNodeIdx)
		{
			ContextSlot& slot = pWheel->m_Slots[nNodeIdx];
			while (!slot.empty())
			{
				context_cb * pTickContext = slot.front();
				__FreeContext(pTickContext);
				slot.pop_front();
			}
		}
		delete[] pWheel->m_Slots;
		delete pWheel;
		m_Wheels[nWheelIdx] = NULL;
	}
	m_ContextMap.clear();
}

const void* TimingWheel::_registerCall(unsigned int delayMilSec, unsigned int interval, Object* object, DelayCall func, void* param, unsigned int tiwce, bool weakReference)
{
	context_cb* context = __AllocContext();
	context->object = object;
	context->ident = (void*)(m_nNextCallIdent++);
	context->func = func;
	context->param = param;
	context->interval = interval;
	context->expireTick = m_ullJiffies + delayMilSec;
	context->twice = 0;
	context->maxTwice = tiwce;
	context->weakReference = weakReference;
	context->removed = false;
	if (!context->weakReference)
	{
		object->retain();
	}
	__AddContext(context);
	m_ContextMap.insert(std::make_pair(context->ident, context));
	return context->ident;
}

void TimingWheel::cancelCall(const void* ident)
{
	ContextMap::iterator itcx = m_ContextMap.find(ident);
	if (itcx != m_ContextMap.end())
	{
		context_cb* context = itcx->second;
		if (ident == m_pCurrCallIdent)
		{	// 当前正在处理中的节点，仅标记为移出状态
			context->removed = true;
			return;
		}
		Wheel& rWheel = *m_Wheels[context->wheelIdx];
		ContextSlot& rSlot = rWheel.m_Slots[context->slotIdx];
		for (ContextSlot::iterator itnd = rSlot.begin(); itnd != rSlot.end(); ++itnd)
		{
			context_cb* cx = *itnd;
			if (!cx || cx->ident != ident)
				continue;
			if (!cx->weakReference && cx->object)
			{
				cx->object->release();
				cx->object = NULL;
			}
			rSlot.erase(itnd);
			m_ContextMap.erase(itcx);
			__FreeContext(cx);
			return;
		} // end for
	}
}

// 插入context
void TimingWheel::__AddContext(context_cb* context)
{
#ifdef _DEBUG
	unsigned int uOldWIdx = context->wheelIdx, uOldSIdx = context->slotIdx;
	__GetTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies),
		context->wheelIdx, context->slotIdx);
	// ATTENTION :	再次注册的context_cb如果newWheelIdx == oldWheelIdx && newSlotIdx == oldSlotIdx
	//				在逻辑上会出现“死循环”，按常理是不允许的，万一触发则代表算法有问题
	assert(context->twice == 0 || (context->wheelIdx != uOldWIdx || context->slotIdx != uOldSIdx));
#else
	__GetTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies), 
					context->wheelIdx, context->slotIdx);
#endif
	m_Wheels[context->wheelIdx]->m_Slots[context->slotIdx].push_back(context);
}

void	TimingWheel::__GetTickPos(unsigned int expireTick, unsigned int leftTick, unsigned int &rnWheelIdx, unsigned int &rnSlotIdx)
{
	rnWheelIdx = rnSlotIdx = 0;
	for (int wIdx = WHEEL_COUNT - 1; wIdx > -1; --wIdx)
	{
		Wheel& rWheels = *m_Wheels[wIdx];
		if (leftTick >= rWheels.uSlotUnitTick)
		{
			rnWheelIdx = wIdx;
			rnSlotIdx = __GetTickSlotIdx(expireTick, leftTick, wIdx);
			return;
		}
	}
	// ATTENTION :	如果代码访问到这里，代表注册了一个nTimeLeft = 0的接口
	//				如此以来很有可能形成一个逻辑"死循环"，在registCall的时候可以加以防范
}

int		TimingWheel::__GetTickSlotIdx(unsigned int expireTick, unsigned int leftTick, unsigned int nWheelIdx)
{
	Wheel& rWheel = *m_Wheels[nWheelIdx];
	return (expireTick >> rWheel.uSlotUnitBit) & rWheel.uSlotMask;
}

void TimingWheel::__CascadeTimers(Wheel& rWheel)
{
	ContextSlot& slot = rWheel.m_Slots[rWheel.m_uSlotIdx];
	while(!slot.empty())
	{
		context_cb* context = slot.front();
		slot.pop_front();
		__MoveContext(context);
	}
	rWheel.m_uSlotIdx = ((++rWheel.m_uSlotIdx) & rWheel.uSlotMask);
}

void	TimingWheel::__Callback(context_cb& context)
{
	if (context.object && context.func)
	{
		// FIXME : try{
		(context.object->*context.func)(context.param, context.twice);
		// FIXME : } catch(...) { /* do something */ }
	}
}

void TimingWheel::update(unsigned int elapse)
{
	Wheel& rWheelRoot = *m_Wheels[0];

	while (elapse > 0)
	{
		// 已经走过一轮，将更高层wheel中的数据“转移”至下层
		if (0 == rWheelRoot.m_uSlotIdx)
		{
			int n = 1;
			do {
				__CascadeTimers(*m_Wheels[n]);
			} while (m_Wheels[n]->m_uSlotIdx == 1 && ++n < WHEEL_COUNT);
		}

		// 处理root wheel中当前tick所有callback
		ContextSlot& rSlot = rWheelRoot.m_Slots[rWheelRoot.m_uSlotIdx];
		while (!rSlot.empty())
		{
			context_cb * context = rSlot.front();
			if (!context->removed)
			{
				m_pCurrCallIdent = context->ident;
				__Callback(*context);
				m_pCurrCallIdent = NULL;
				++context->twice;
				if (context->maxTwice && context->twice >= context->maxTwice)
					context->removed = true;
			}
			if (context->removed)
			{
				if (!context->weakReference && context->object)
				{
					context->object->release();
					context->object = NULL;
				}
				m_ContextMap.erase(context->ident);
				__FreeContext(context);
				context = NULL;
			}
			else
			{
				context->expireTick += context->interval;
				__RepushContext(context);
			}
			rSlot.pop_front();
		}

		++(m_ullJiffies);
		rWheelRoot.m_uSlotIdx = (rWheelRoot.m_uSlotIdx + 1) & rWheelRoot.uSlotMask;
		--elapse;
	}
}

}