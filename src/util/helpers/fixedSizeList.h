#pragma once

#include<array>

template<typename T, uint32 maxElements, bool checkMaxSize = true>
class FixedSizeList
{
public:
	std::array<T, maxElements> m_elementArray;
	int count = 0;

	void add(T n)
	{
		if (checkMaxSize && count >= maxElements)
			return;
		m_elementArray[count] = n;
		count++;
	}

	void addUnique(T n)
	{
		if (checkMaxSize && count >= maxElements)
			return;
		for (int i = 0; i < count; i++)
		{
			if (m_elementArray[i] == n)
				return;
		}
		m_elementArray[count] = n;
		count++;
	}

	void remove(T n)
	{
		for (int i = 0; i < count; i++)
		{
			if (m_elementArray[i] == n)
			{
				m_elementArray[i] = m_elementArray[count - 1];
				count--;
				return;
			}
		}
	}

	bool containsAndRemove(T n)
	{
		for (int i = 0; i < count; i++)
		{
			if (m_elementArray[i] == n)
			{
				m_elementArray[i] = m_elementArray[count - 1];
				count--;
				return true;
			}
		}
		return false;
	}

	sint32 find(T n)
	{
		for (int i = 0; i < count; i++)
		{
			if (m_elementArray[i] == n)
			{
				return i;
			}
		}
		return -1;
	}

private:
};

