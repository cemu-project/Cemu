#pragma once

#include <wx/clntdata.h>

template <typename T>
class wxCustomData : public wxClientData
{
public:
	wxCustomData()
		:  m_data({}) {}

	wxCustomData(T data)
		:  m_data(std::move(data)) { }
	
	const T& GetData() const { return m_data; }

	/// <summary>
	/// obtains ownership of the data
	/// </summary>
	/// <returns>returns the stored data</returns>
	T get() { return std::move(m_data); }

	/// <summary>
	/// access to the data without obtaining ownership
	/// </summary>
	/// <returns>returns a reference to the stored data</returns>
	T& ref() { return m_data; }

	bool operator==(const T& o) const { return m_data == o.m_data; }
	bool operator!=(const T& o) const { return !(*this == o); }

private:
	T m_data;
};

//template <typename T>
//class wxCustomObject : public wxObject
//{
//public:
//	wxCustomObject(T data)
//		:  m_data(std::move(data)) { }
//
//	const T& GetData() const { return m_data; }
//
//	bool operator==(const T& o) const { return m_data == o.m_data; }
//	bool operator!=(const T& o) const { return !(*this == o); }
//private:
//	T m_data;
//};
