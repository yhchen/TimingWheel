#ifndef __SG2D_OBJECT_H__
#define __SG2D_OBJECT_H__

#include <atomic>
#include <assert.h>

namespace Timer
{
/**
* ������shared_ptr����ָ���ʵ�֣����ü���Ϊ0ʱ����������
* retain���������ü�����release�������ü�������������Ĭ��
* ���ü���Ϊ1��
*/
class Object
{
private:
	/*
	 * ���ü��������λ��ʾ����ɾ����ǣ�Ϊ��ֹ����������������������retain��release������
	 * �ᵼ�¶�ε�������������Ϊ��������⣬���λ����ɾ���б�ʶ������λ���ڼ�¼��ʵ���ü���
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
	//��ȡ���ü���
	inline int getRefer()
	{
		return m_nRefer._My_val & 0x7FFFFFFF;
// 		return m_nRefer.fetch_and(0x7FFFFFFF, std::memory_order_acquire);
	}
	//��������
	inline int retain()
	{
		m_nRefer.fetch_add(1, std::memory_order_release);
		return getRefer();
	}
	//ȡ�����ã�����������ֵΪ0ʱ����������
	inline int release()
	{
#ifdef DEBUG
		assert((m_nRefer & 0x7FFFFFFF) > 0);
#endif
		unsigned int ret = m_nRefer.fetch_sub(1, std::memory_order_release);
		if (ret == 0)
		{
			m_nRefer.fetch_or(0x80000000, std::memory_order_release);//����ɾ�����
			delete this;
		}
		return getRefer();
	}
};
}

#endif