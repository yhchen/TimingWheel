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
		unsigned int	m_nBitCount;			//�ڵ��ֽ���
		unsigned int	m_nNodeCount;			//��ǰ�ֽڵ�����( POW(�ڵ��ֽ���) )
		unsigned int	m_nTickUnitBit;			//ÿ��ʱ��Ƭ�����ʱ�䵥λ(log 2(m_nTickUnit) )
		unsigned int	m_nTickUnit;			//ÿ��ʱ��Ƭ�����ʱ�䵥λ
		unsigned int	m_nSlotMask;			//��ǰ��㡰ȡģ����
		unsigned int	m_nSlotIdx;				//��¼��ǰʱ�����ָ���slotIdx
		ContextSlot*	m_Slots;
	};

private:
	// Ĭ��ʹ�� new �� delete ����context_cb�ڴ棬������չΪ����ع�������Ч��
	virtual context_cb* __AllocContext();
	virtual void __FreeContext(context_cb* cx);

	// ����context
	inline void __AddContext(context_cb* context);

	// ����ʱ�������λ�±�
	void	__GetTickPos(unsigned int expireTick, unsigned int leftTick, unsigned int &nWheelIdx, unsigned int &nSlotIdx);
	int		__GetTickSlotIdx(unsigned int expireTick, unsigned int leftTick, unsigned int nWheelIdx);

	void	__CascadeTimers(Wheel& wheel);
	void	__Callback(context_cb& context);

public:	// FIXME : remove where
	unsigned long long	m_ullJiffies;			// ���ŵĺ�����
	Wheel*				m_Wheels[WHEEL_COUNT];	// ʱ����
	ContextMap			m_ContextMap;		// ��¼�ص��ĻỰ��Ӧ��ϵ
	unsigned int		m_nTotalTickSize;	// ��¼��ǰע��ص���������(use for debug)
	unsigned int		m_nNextCallIdent;	// ÿ��context_cb��ΨһIdent��ʶ
};

}