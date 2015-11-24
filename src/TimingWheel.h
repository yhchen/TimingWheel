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

	//ע����ʱ����
	//����delayMilSec��ʾ�ӳ��¼�����λ�Ǻ��룬ָ��Ϊ0��ʾ����һ��TimeCall�����£���һ֡��ʱ���е��ã�
	//����objectΪ���ö���
	//����DELAYFUNCΪDelayCall���͵Ļص�������
	//����paramΪ�ص�����������
	//REMARKS����ʱ������ִ�к���Զ���ɾ���������ֶ��Ƴ���
	//RETURN����������ֵ��ʾע���ĵ��ö����־ֵ����ȡ������ʱ��ʹ�ô�ֵ���ж���ȷ�ϡ�
	template<typename OBJECTTYPE, typename DELAYCALLFN>
	inline const void* delayCall(unsigned int delayMilSec, OBJECTTYPE* object, DELAYCALLFN func, void* param, bool weakReference = false)
	{
		return delayIntervalCall(delayMilSec, 0, object, func, param, 1, weakReference);
	}

	//ע�ᶨʱ����
	//����interval��ʾ���ü������λ�Ǻ��룻
	//����objectΪ���ö���
	//����DELAYFUNCΪDelayCall���͵Ļص�������ָ��Ϊ0���ʾÿ��TimeCall�õ�����ʱ��ÿ֡���������е��ã�
	//����paramΪ�ص�����������
	//����twiceΪ���ô�����ָ��Ϊ0��ʾ���޴������ã������ʾ������õĴ����������ô����ﵽ
	//ָ����ֵ�󣬴˶�ʱ���ý����Ƴ���
	//RETURN����������ֵ��ʾע���ĵ��ö����־ֵ����ȡ������ʱ��ʹ�ô�ֵ���ж���ȷ�ϡ�
	template<typename OBJECTTYPE, typename INTERVALCALLFN>
	inline const void* intervalCall(unsigned int interval, OBJECTTYPE* object, INTERVALCALLFN func, void* param, unsigned int twice = 0, bool weakReference = false)
	{
		return delayIntervalCall(interval, interval, object, func, param, twice, weakReference);
	}

	//ע���ӳٶ�ʱ����
	//����delay��ʾ�ӳ�ʱ�䣬��λ�Ǻ��룬ָ��Ϊ0��ʾ����һ��TimeCall�����£���һ֡��ʱ���е��ã�
	//����interval��ʾ���ü������λ�Ǻ��룬ָ��Ϊ0���ʾÿ��TimeCall�õ�����ʱ��ÿ֡���������е��ã�
	//����objectΪ���ö���
	//����DELAYFUNCΪDelayCall���͵Ļص�������ָ��Ϊ0���ʾÿ��TimeCall�õ�����ʱ��ÿ֡���������е��ã�
	//����paramΪ�ص�����������
	//����twiceΪ���ô�����ָ��Ϊ0��ʾ���޴������ã������ʾ������õĴ����������ô����ﵽָ����ֵ�󣬴˶�ʱ���ý����Ƴ���
	//RETURN����������ֵ��ʾע���ĵ��ö����־ֵ����ȡ������ʱ��ʹ�ô�ֵ���ж���ȷ�ϡ�
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
	static const int WHEEL_COUNT = 5;		//ʱ��������
	static const int WHEEL_ROOT_BIT = 8;	//��0��ʱ����2(param)����
	static const int WHEEL_NODE_BIT = 6;	//����ʱ����2(param)����

	struct context_cb
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
	};

	typedef std::list<context_cb*> 	ContextSlot;
	typedef unordered_map<const void*, context_cb*> ContextMap;

	struct Wheel
	{
		Wheel(	unsigned int bitCnt, 
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
		const unsigned int	uSlotUnitBit;	//ÿ��ʱ��Ƭ�����ʱ�䵥λ(log 2(m_nTickUnit) )
		const unsigned int	uSlotUnitTick;	//ÿ��ʱ��Ƭ�����ʱ�䵥λ
		const unsigned int	uSlotMask;		//��ǰ��㡰ȡģ����
		unsigned int	m_uSlotIdx;			//��¼��ǰʱ�����ָ���slotIdx
		ContextSlot*	m_Slots;
	};

private:
	// Ĭ��ʹ�� new �� delete ����context_cb�ڴ棬������չΪ����ع�������Ч��
	virtual context_cb* __AllocContext() { return (context_cb*)malloc(sizeof(context_cb)); }
	virtual void __FreeContext(context_cb* cx) { if (cx) free(cx); }

	// ����context
	inline void __AddContext(context_cb* context);
	inline void __MoveContext(context_cb* context) { __AddContext(context); }
	inline void __RepushContext(context_cb* context) { __AddContext(context); }

	// ����ʱ�������λ�±�
	void	__GetTickPos(unsigned int expireTick, unsigned int leftTick, unsigned int &nWheelIdx, unsigned int &nSlotIdx);
	int		__GetTickSlotIdx(unsigned int expireTick, unsigned int leftTick, unsigned int nWheelIdx);

	void	__CascadeTimers(Wheel& wheel);
	void	__Callback(context_cb& context);

public:
	unsigned long long	m_ullJiffies;			// ���ŵĺ�����
	Wheel*				m_Wheels[WHEEL_COUNT];	// ʱ����
	ContextMap			m_ContextMap;		// ��¼�ص��ĻỰ��Ӧ��ϵ
	size_t				m_nNextCallIdent;	// ÿ��context_cb��ΨһIdent��ʶ
	const void*			m_pCurrCallIdent;	// ��ǰ���ڵ����е�ident
};

}