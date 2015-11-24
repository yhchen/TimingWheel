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
		unsigned int uBitCount = ((wIdx == 0) ? WORK_WHEEL_BIT : ASSIST_WHEEL_BIT);
		unsigned int uSlotCount = 1 << uBitCount;
		unsigned int uSlotUnitBit = ((wIdx == 0) ? 0 : (WORK_WHEEL_BIT + ASSIST_WHEEL_BIT * (wIdx-1)));
		unsigned int uSlotUnitTick = ((wIdx == 0) ? 0 : 1 << uSlotUnitBit);
		unsigned int uSlotMask = uSlotCount - 1;
		Wheel* pWheel = new Wheel(uBitCount, uSlotCount, uSlotUnitBit, uSlotUnitTick, uSlotMask);
		pWheel->m_Slots = new ContextSlot[pWheel->uSlotCount];
		for (int sIdx = 0; sIdx < pWheel->uSlotCount; ++sIdx)
		{
			TAILQ_INIT(&pWheel->m_Slots[sIdx]);
		}
		m_Wheels[wIdx] = pWheel;
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
			context_cb* context = NULL;
			TAILQ_FOREACH(context, &slot, entry)
			{
				if (context)
				{
					TAILQ_REMOVE(&slot, context, entry);
					__FreeContext(context);
				}
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
	// interval=0�ᵼ���߼�����ѭ��������ǿ�Ʋ�����˷�ʽ
	assert(interval >= 1);
	context->interval = max((unsigned int)1, interval);
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

void	TimingWheel::cancelCall(const void* ident)
{
	ContextMap::iterator itcx = m_ContextMap.find(ident);
	if (itcx == m_ContextMap.end())
		return;	// �Ҳ���context_cb
	context_cb* context = itcx->second;
	if (context == NULL || context->removed == true)
		return;	// ���ݴ�����Ƴ�
	if (ident == m_pCurrCallIdent)
	{	// ��ǰ���ڴ����еĽڵ㣬�����Ϊ�Ƴ�״̬
		context->removed = true;
		return;
	}
	Wheel& rWheel = *m_Wheels[context->wheelIdx];
	ContextSlot& rSlot = rWheel.m_Slots[context->slotIdx];
	if (!context->weakReference && context->object)
	{
		context->object->release();
		context->object = NULL;
	}
	TAILQ_REMOVE(&rSlot, context, entry);
	__FreeContext(context);
	m_ContextMap.erase(itcx);
}

void	TimingWheel::update(unsigned int elapse)
{
	Wheel& rWheelWork = *m_Wheels[0];

	while (elapse > 0)
	{
		// �Ѿ��߹�һ�֣������߲�wheel�е����ݡ�ת�ơ����²�
		if (0 == rWheelWork.m_uSlotIdx)
		{
			int n = 1;
			do {
				__CascadeTimers(*m_Wheels[n]);
			} while (m_Wheels[n]->m_uSlotIdx == 1 && ++n < WHEEL_COUNT);
		}

		// ����work wheel�е�ǰtick����callback
		ContextSlot& rSlot = rWheelWork.m_Slots[rWheelWork.m_uSlotIdx];
		while (!TAILQ_EMPTY(&rSlot))
		{
			context_cb * context = TAILQ_FIRST(&rSlot);
			TAILQ_REMOVE(&rSlot, context, entry);
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
		}

		++(m_ullJiffies);
		rWheelWork.m_uSlotIdx = (rWheelWork.m_uSlotIdx + 1) & rWheelWork.uSlotMask;
		--elapse;
	}
}

// ����context
void	TimingWheel::__AddContext(context_cb* context)
{
#ifdef _DEBUG
	unsigned int uOldWIdx = context->wheelIdx, uOldSIdx = context->slotIdx;
	__GetTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies),
		context->wheelIdx, context->slotIdx);
	// ATTENTION :	�ٴ�ע���context_cb���newWheelIdx == oldWheelIdx && newSlotIdx == oldSlotIdx
	//				���߼��ϻ���֡���ѭ�������������ǲ�����ģ���һ����������㷨������
	assert(context->twice == 0 || (context->wheelIdx != uOldWIdx || context->slotIdx != uOldSIdx));
#else
	__GetTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies), 
					context->wheelIdx, context->slotIdx);
#endif
	ContextSlot& rSlot = m_Wheels[context->wheelIdx]->m_Slots[context->slotIdx];
	TAILQ_INSERT_TAIL(&rSlot, context, entry);
}

void	TimingWheel::__GetTickPos(unsigned long long expireTick, unsigned int leftTick, unsigned int &rnWheelIdx, unsigned int &rnSlotIdx)
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
	// ATTENTION :	���������ʵ��������ע����һ��nTimeLeft = 0�Ľӿ�
	//				����������п����γ�һ���߼�"��ѭ��"����registCall��ʱ����Լ��Է���
}

int		TimingWheel::__GetTickSlotIdx(unsigned long long expireTick, unsigned int leftTick, unsigned int nWheelIdx)
{
	Wheel& rWheel = *m_Wheels[nWheelIdx];
	return (expireTick >> rWheel.uSlotUnitBit) & rWheel.uSlotMask;
}

void	TimingWheel::__CascadeTimers(Wheel& rWheel)
{
	ContextSlot& rSlot = rWheel.m_Slots[rWheel.m_uSlotIdx];
	while (!TAILQ_EMPTY(&rSlot))
	{
		context_cb* context = TAILQ_FIRST(&rSlot);
		TAILQ_REMOVE(&rSlot, context, entry);
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

}