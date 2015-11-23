#ifndef __SG2D_OBJECT_H__
#define __SG2D_OBJECT_H__

#include "windows.h"
#include <assert.h>

inline int lock_and(volatile unsigned int &n, int value)
{
	return _InterlockedAnd((long*)&n, value);
}
inline unsigned int lock_inc(volatile unsigned int &n)
{
	return InterlockedIncrement(&n);
}
inline unsigned int lock_or(volatile unsigned int &n, unsigned int value)
{
	return _InterlockedOr((long*)&n, value);
}
inline int lock_dec(volatile unsigned int &n)
{
	return InterlockedDecrement((long*)&n);
}


/**
* ���������࣬����SG2D����Ļ��ࡣ
* ������������ü���������������ü���Ϊ0ʱ�������������
* ���������ָ���ͨ������retain���Ӷ������ü�����ɾ������
* ָ�����ú�ʹ��release���ٶ������ü�����
*/
class Object
{
private:
	//���ü��������λ��ʾɾ����ǣ����������ڱ�����ʱ���λ�ᱻ��1������31λ��ʾʵ�����ü�����
	//ʹ�����ü���������������ڴ���һ����Ҫ������������Σ�������Ϊ���ü���Ϊ0����Ҫ��ɾ������
	//ʱ�ڶ�������������п��ܻ�������˶������Ӻͼ������ã�����һ���������ٵĶ����������ò�
	//��������ʱ���������ü����������������ԭ��release�������жϵݼ����ú����Ӧ�����٣���
	//���ٴη���Զ�������ٲ��������´˶������ٶ�Ρ���ˣ�Ϊ���������ر����������������ܹ�
	//ʶ��������Ƿ��������ٴӶ����ڵݼ����ü���ʱ���м��ʶ�𲢱��������ٶ���Ĳ�������˽���
	//�����ü�����Ա�����λ�������������ġ��������١��ı�ǡ�
	unsigned int m_nRefer;

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
		return lock_and(m_nRefer, 0x7FFFFFFF);
	}
	//��������
	inline int retain()
	{
		return lock_inc(m_nRefer) & 0x7FFFFFFF;
	}
	//ȡ�����ã�����������ֵΪ0ʱ����������
	inline int release()
	{
#ifdef _DEBUG
		assert((m_nRefer & 0x7FFFFFFF) > 0);
#endif
		unsigned int ret = lock_dec(m_nRefer);
		if (ret == 0)
		{
			lock_or(m_nRefer, 0x80000000);//����ɾ�����
			delete this;
		}
		return ret & 0x7FFFFFFF;
	}
};

#endif