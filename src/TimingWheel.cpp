#include <algorithm>
#include "Object.h"
#include "TimingWheel.h"

namespace Timer
{
	struct TimingWheel::context_cb
	{
		const void* ident;	// Ψһ��ݱ�ʶ
		Object*	object;		// callback object
		DelayCall func;		// callback����ָ��
		void* param;		// callback����(�����д���)
		unsigned long long interval;	// callbackƵ��
		unsigned long long expireTick;	// �´�callbackʱ��(ms)
		unsigned int twice;		// ��ǰcallback����
		unsigned int maxTwice;	// ���callback����(0-������)
		unsigned int wheelIdx;	// m_wheels[wheel]
		unsigned int slotIdx;	// m_wheels[wheel]->m_slots[slot]
		bool weakReference;		// ������(���й�object������������)
		bool removed;			// �Ƴ����
		TAILQ_ENTRY(context_cb)	entry;	//fifoָ��
	};
	
	struct TimingWheel::Wheel
	{
		Wheel(unsigned int bitCnt,
			unsigned int slotCnt,
			unsigned int slotUnitBit,
			unsigned int slotUnitTick,
			unsigned int slotMask
			) : m_uSlotIdx(0), m_Slots(NULL), uBitCount(bitCnt),
			uSlotCount(slotCnt), uSlotUnitBit(slotUnitBit),
			uSlotUnitTick(slotUnitTick), uSlotMask(slotMask)
		{ }
		const unsigned int	uBitCount;		//�ڵ��ֽ���
		const unsigned int	uSlotCount;		//��ǰ�ֽڵ�����( POW(�ڵ��ֽ���) )
		const unsigned int	uSlotUnitBit;	//ÿ��slot�ֽ���(log 2(uSlotUnitTick) )
		const unsigned int	uSlotUnitTick;	//ÿ��slotʱ�䵥λ
		const unsigned int	uSlotMask;		//��ǰwheel��ȡģ����
		unsigned int	m_uSlotIdx;			//��¼��ǰ������slotIdx
		ContextSlot*	m_Slots;
	};
	
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
			for (unsigned int sIdx = 0; sIdx < pWheel->uSlotCount; ++sIdx)
			{
				TAILQ_INIT(&pWheel->m_Slots[sIdx]);
			}
			m_Wheels[wIdx] = pWheel;
		}
	}
	
	TimingWheel::~TimingWheel()
	{
		removeAllCall();
		// �ڴ��ͷ�
		for (int nWheelIdx = 0; nWheelIdx < WHEEL_COUNT; ++nWheelIdx)
		{
			Wheel* pWheel = m_Wheels[nWheelIdx];
			delete[] pWheel->m_Slots;
			delete pWheel;
			m_Wheels[nWheelIdx] = NULL;
		}
	}
	
	const void* TimingWheel::_registerCall(unsigned int delayMilSec, unsigned int interval, Object* object, DelayCall func, void* param, unsigned int twice, bool weakReference)
	{
// 		wylib::sync::lock::CSafeLock locking(&m_lock);
		context_cb* context = allocContext();
		context->object = object;
		context->ident = (void*)(m_nNextCallIdent++);
		context->func = func;
		context->param = param;
		// interval=0�ᵼ���߼�����ѭ��������ǿ�Ʋ�����˷�ʽ
		assert(twice != 0 || interval >= 1);
		context->interval = max((unsigned int)1, interval);
		context->expireTick = m_ullJiffies + max((unsigned int)1, delayMilSec);
		context->twice = 0;
		context->maxTwice = twice;
		context->weakReference = weakReference;
		context->removed = false;
		if (!context->weakReference)
		{
			object->retain();
		}
		addContext(context);
		m_ContextMap.insert(std::make_pair(context->ident, context));
		return context->ident;
	}
	
	void TimingWheel::cancelCall(const void* ident)
	{
// 		wylib::sync::lock::CSafeLock locking(&m_lock);
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
			Object* obj = context->object;
			context->object = NULL;
			obj->release();
		}
		TAILQ_REMOVE(&rSlot, context, entry);
		freeContext(context);
		m_ContextMap.erase(itcx);
	}
	
	void TimingWheel::update(unsigned int elapse)
	{
// 		wylib::sync::lock::CSafeLock locking(&m_lock);
		Wheel& rWheelWork = *m_Wheels[0];
	
		while (elapse > 0)
		{
			// �Ѿ��߹�һ�֣������߲�wheel�е����ݡ�ת�ơ����²�
			if (0 == rWheelWork.m_uSlotIdx)
			{
				int n = 1;
				do {
					cascadeTimers(*m_Wheels[n]);
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
					if (context->object && context->func)
					{
						callback(context);
					}
					m_pCurrCallIdent = NULL;
					++context->twice;
					if (context->maxTwice && context->twice >= context->maxTwice)
						context->removed = true;
				}
				if (context->removed)
				{
					if (!context->weakReference && context->object)
					{
						Object* obj = context->object;
						context->object = NULL;
						obj->release();
					}
					m_ContextMap.erase(context->ident);
					freeContext(context);
					context = NULL;
				}
				else
				{
					context->expireTick += context->interval;
					repushContext(context);
				}
			}
	
			++(m_ullJiffies);
			rWheelWork.m_uSlotIdx = (rWheelWork.m_uSlotIdx + 1) & rWheelWork.uSlotMask;
			--elapse;
		}
	}
	
	void TimingWheel::removeAllCall()
	{	
// 		wylib::sync::lock::CSafeLock locking(&m_lock);
		for (int wIdx = 0; wIdx < WHEEL_COUNT; ++wIdx)
		{
			ContextSlot* slotList = m_Wheels[wIdx]->m_Slots;
			for (int sIdx = m_Wheels[wIdx]->uSlotCount - 1; sIdx > -1; --sIdx)
			{
				ContextSlot& rSlot = slotList[sIdx];
				while (!TAILQ_EMPTY(&rSlot))
				{
					context_cb* context = TAILQ_FIRST(&rSlot);
					TAILQ_REMOVE(&rSlot, context, entry);
					if (context->object && !context->weakReference)
					{
						Object* obj = context->object;
						//ע��Ϊ�����ڶ��������ڲ��ٴε���cancelCall�����ظ��������˴������������Ƴ�״̬
						context->removed = true;
						context->object = NULL;
						obj->release();
					}
					freeContext(context);
				}
			}
		}
		m_ContextMap.clear();
	}
	
	TimingWheel::context_cb* TimingWheel::allocContext()
	{
		return (context_cb*)malloc(sizeof(context_cb));
	}
	
	void TimingWheel::freeContext(context_cb* cx) 
	{
		if (cx) free(cx);
	}
	
	void TimingWheel::callback(context_cb* context)
	{
// 		__BEGIN_SEH_CATCH__
		(context->object->*context->func)(context->param, context->twice);
// 		__END_SEH_CATCH__(0)
	}
	
	// ����context
	void TimingWheel::addContext(context_cb* context)
	{
	#ifdef _DEBUG
		unsigned int uOldWIdx = context->wheelIdx, uOldSIdx = context->slotIdx;
		getTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies),
			context->wheelIdx, context->slotIdx);
		// ATTENTION :	�ٴ�ע���context_cb���newWheelIdx == oldWheelIdx && newSlotIdx == oldSlotIdx
		//				���߼��ϻ���֡���ѭ�������������ǲ�����ģ���һ����������㷨������
		assert(context->twice == 0 || (context->wheelIdx != uOldWIdx || context->slotIdx != uOldSIdx));
	#else
		getTickPos(context->expireTick, max((unsigned long long)0, context->expireTick - m_ullJiffies), 
						context->wheelIdx, context->slotIdx);
	#endif
		ContextSlot& rSlot = m_Wheels[context->wheelIdx]->m_Slots[context->slotIdx];
		TAILQ_INSERT_TAIL(&rSlot, context, entry);
	}
	
	void TimingWheel::getTickPos(unsigned long long expireTick, unsigned long long leftTick, unsigned int &rnWheelIdx, unsigned int &rnSlotIdx)
	{
		rnWheelIdx = rnSlotIdx = 0;
		for (int wIdx = WHEEL_COUNT - 1; wIdx > -1; --wIdx)
		{
			Wheel& rWheels = *m_Wheels[wIdx];
			if (leftTick >= rWheels.uSlotUnitTick)
			{
				rnWheelIdx = wIdx;
				rnSlotIdx = getTickSlotIdx(expireTick, leftTick, wIdx);
				return;
			}
		}
		// ATTENTION :	���������ʵ��������ע����һ��nTimeLeft = 0�Ľӿ�
		//				����������п����γ�һ���߼�"��ѭ��"����registCall��ʱ����Լ��Է���
	}
	
	int TimingWheel::getTickSlotIdx(unsigned long long expireTick, unsigned long long leftTick, unsigned int nWheelIdx)
	{
		Wheel& rWheel = *m_Wheels[nWheelIdx];
		return (expireTick >> rWheel.uSlotUnitBit) & rWheel.uSlotMask;
	}
	
	void TimingWheel::cascadeTimers(Wheel& rWheel)
	{
		ContextSlot& rSlot = rWheel.m_Slots[rWheel.m_uSlotIdx];
		while (!TAILQ_EMPTY(&rSlot))
		{
			context_cb* context = TAILQ_FIRST(&rSlot);
			TAILQ_REMOVE(&rSlot, context, entry);
			moveContext(context);
		}
		rWheel.m_uSlotIdx = ((++rWheel.m_uSlotIdx) & rWheel.uSlotMask);
	}
} // namespace SG2DEX
