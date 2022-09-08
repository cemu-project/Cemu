#include "FSPath.h"

#ifdef BOOST_OS_UNIX
FSPath operator/ (const FSPath& lhs, const FSPath& rhs)
{
	FSPath res{lhs};
	res /= rhs;
	return res;
}

FSPath& FSPath::operator/ (const FSPath & rhs)
{
	*this /= rhs;
	return *this;
}

FSPath& FSPath::operator/= (const FSPath & rhs)
{
	// todo: implement caching.

	// explore directories recursively and find the matching cases.
	fs::path relPath = rhs.relative_path();
	fs::path correctedPath = empty() ? rhs.root_path() : *this;

	bool found;
	for (auto const &it : relPath)
	{
		found = false;
		std::error_code listErr;
		for (auto const& dirEntry : fs::directory_iterator{correctedPath, listErr})
		{
			fs::path entryName = dirEntry.path().filename();
			if (boost::iequals(entryName, it))
			{
				correctedPath /= entryName;
				found = true;
				break;
			}
		}

		// if we can't iterate directory, just copy the original case.
		if(listErr || !found)
		{
			correctedPath /= it;
		}
	}

	*this = correctedPath;
	return *this;
}

#endif
