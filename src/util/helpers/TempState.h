#pragma once

template<typename TCtor, typename TDtor>
class TempState
{
public:
	TempState(TCtor ctor, TDtor dtor)
		: m_dtor(std::move(dtor))
	{
		ctor();
	}

	~TempState()
	{
		m_dtor();
	}

private:
	TDtor m_dtor;
};
