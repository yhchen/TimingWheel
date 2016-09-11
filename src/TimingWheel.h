#pragma once
#include <unordered_map>
#include <assert.h>
#ifdef _WIN32
#	include "queue.h"
#else
#	include <sys/queue.h>
#endif

namespace Timer
{
	using namespace std;

	class Object;
	
	class TimingWheel
	{
	public:
		// callback������ʽ
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
	
		const void* _registerCall(unsigned int delayMilSec, unsigned int interval, Object* object, DelayCall func, void* param, unsigned int twice, bool weakReference);
		void	cancelCall(const void* ident);	// ȡ��ident callback
		void	update(unsigned int elapse/* tick */);	//֪ͨ����tickCount(ms) һ������ѭ���е���
		void	removeAllCall();	// �Ƴ�����callback	
	
	protected:
		static const int WHEEL_COUNT = 5;		//ʱ��������([1]������ + [n-1]������)
		static const int WORK_WHEEL_BIT = 8;	//������2(param)����
		static const int ASSIST_WHEEL_BIT = 6;	//������2(param)����
	
		struct context_cb;
		struct Wheel;
	
		TAILQ_HEAD(ContextSlot, context_cb);
		typedef unordered_map<const void*, context_cb*> ContextMap;
	
	private:
		// Ĭ��ʹ�� new �� delete ����context_cb�ڴ棬������չΪ����ع�������Ч��
		virtual context_cb* allocContext();
		virtual void freeContext(context_cb* cx);
		// �ص�
		void callback(context_cb* context);
		
		// ����context
		void addContext(context_cb* context);
		inline void moveContext(context_cb* context) { addContext(context); }
		inline void repushContext(context_cb* context) { addContext(context); }
		// ����ʱ�������λ�±�
		void getTickPos(unsigned long long expireTick, unsigned long long leftTick, unsigned int &nWheelIdx, unsigned int &nSlotIdx);
		int	getTickSlotIdx(unsigned long long expireTick, unsigned long long leftTick, unsigned int nWheelIdx);
		// �ƶ�assist wheel��context_cb
		void cascadeTimers(Wheel& wheel);
	private:
		Wheel*				m_Wheels[WHEEL_COUNT];	// ʱ����
		const void*			m_pCurrCallIdent;	// ��ǰ���ڵ����е�ident
		unsigned long long	m_ullJiffies;			// ���ŵĺ�����
		ContextMap			m_ContextMap;		// ��¼�ص��ĻỰ��Ӧ��ϵ
// 		wylib::sync::lock::CCSLock	m_lock;		// ����������֤registCall�̰߳�ȫ
		size_t				m_nNextCallIdent;	// ÿ��context_cb��ΨһIdent��ʶ
		bool				m_boRemoveAllFlag;//������
	};
}