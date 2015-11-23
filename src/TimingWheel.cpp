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
TimingWheel::TimingWheel() :m_ullJiffies(0), m_nTotalTickSize(0)
{
	m_nNextCallIdent = 1;
	for (int i = 0; i < WHEEL_COUNT; ++i)
	{
		m_Wheels[i] = new Wheel;
		//OFFSETVAL(Wheel, m_nBitCount, unsigned int, m_Wheels[i]) = ((i == 0) ? WHEEL_ROOT_BIT : WHEEL_NODE_BIT);
		m_Wheels[i]->m_nBitCount = ((i==0) ? WHEEL_ROOT_BIT : WHEEL_NODE_BIT);
		m_Wheels[i]->m_nNodeCount = 1 << m_Wheels[i]->m_nBitCount;
		m_Wheels[i]->m_nTickUnitBit = ((i==0) ? 0 : (WHEEL_ROOT_BIT + WHEEL_NODE_BIT * (i-1)));
		m_Wheels[i]->m_nTickUnit = ((i==0) ? 1 : m_Wheels[i-1]->m_nTickUnit << m_Wheels[i-1]->m_nBitCount);
		m_Wheels[i]->m_nSlotMask = m_Wheels[i]->m_nNodeCount - 1;
		m_Wheels[i]->m_Slots = new ContextSlot[m_Wheels[i]->m_nNodeCount];
	}
}

TimingWheel::~TimingWheel()
{
	for (int nWheelIdx = 0; nWheelIdx < WHEEL_COUNT; ++nWheelIdx)
	{
		Wheel* pWheel = m_Wheels[nWheelIdx];
		for (unsigned int nNodeIdx = 0; nNodeIdx < pWheel->m_nNodeCount; ++nNodeIdx)
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

TimingWheel::context_cb* TimingWheel::__AllocContext()
{
	return (context_cb*)malloc(sizeof(context_cb));
}

void TimingWheel::__FreeContext(context_cb* cx)
{
	free(cx);
}

// 插入context
void TimingWheel::__AddContext(context_cb* context)
{
	unsigned int nWheelIdx = 0;
	unsigned int nNodeIdx = 0;
	__GetTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies), nWheelIdx, nNodeIdx);

	m_Wheels[nWheelIdx]->m_Slots[nNodeIdx].push_back(context);
}

void	TimingWheel::__GetTickPos(unsigned int expireTick, unsigned int leftTick, unsigned int &rnWheelIdx, unsigned int &rnSlotIdx)
{
	rnWheelIdx = rnSlotIdx = 0;
	for (int wIdx = WHEEL_COUNT - 1; wIdx > -1; --wIdx)
	{
		Wheel& rWheels = *m_Wheels[wIdx];
		if (leftTick >= rWheels.m_nTickUnit)
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
	return (expireTick >> rWheel.m_nTickUnitBit) & rWheel.m_nSlotMask;
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

void TimingWheel::cancelCall(const void* id)
{
	// FIXME : add code here
}

void TimingWheel::__CascadeTimers(Wheel& rWheel)
{
	ContextSlot& slot = rWheel.m_Slots[rWheel.m_nSlotIdx];
	while(!slot.empty())
	{
		context_cb* context = slot.front();
		slot.pop_front();
		__AddContext(context);
	}
	rWheel.m_nSlotIdx = (rWheel.m_nSlotIdx + 1)&rWheel.m_nSlotMask;
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
	std::string strTickName;
	Wheel& rWheelRoot = *m_Wheels[0];

	while (elapse > 0)
	{
		if (0 == rWheelRoot.m_nSlotIdx)
		{
			int n = 1;
			do {
				__CascadeTimers(*m_Wheels[n]);
			} while (m_Wheels[n]->m_nSlotIdx == 1 && ++n < WHEEL_COUNT);
		}

		ContextSlot& rSlot = rWheelRoot.m_Slots[rWheelRoot.m_nSlotIdx];

		while (!rSlot.empty())
		{
			context_cb * context = rSlot.front();
			if (context->removed)
			{
				delete context;
				context = NULL;
				--m_nTotalTickSize;
			}
			else
			{
				__Callback(*context);
				++context->twice;
				if (context->maxTwice && context->twice >= context->maxTwice)
					context->removed = true;
				if (context->removed)
				{
					delete context;
					context = NULL;
					--m_nTotalTickSize;
				}
				else
				{
					context->expireTick += context->interval;
					__AddContext(context);
				}
			}

			rSlot.pop_front();
		}

		++(m_ullJiffies);
		rWheelRoot.m_nSlotIdx = (rWheelRoot.m_nSlotIdx + 1) & rWheelRoot.m_nSlotMask;
		--elapse;
	}
}

}