#include "FSPath.h"

#ifdef BOOST_OS_UNIX
FSPath operator/ (const FSPath& lhs, const FSPath& rhs)
{
	FSPath res{lhs};
	res /= FSPath{rhs};
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

	// helper function to convert a path's alphabet characters to lowercase.
	auto static lowercase_path = [](fs::path const & path)
	{
		std::string string = path.string();
		for (auto& i : string)
		{
			i = std::isalpha(i) ? std::tolower(i) : i;
		}
		return string;
	};

	bool found;
	for (auto const &it : relPath)
	{
		found = false;
		std::error_code listErr;
		for (auto const& dirEntry : fs::directory_iterator{correctedPath, listErr})
		{
			fs::path entryName = dirEntry.path().filename();
			if (lowercase_path(entryName) == lowercase_path(it))
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

	*this = FSPath(correctedPath);
	return *this;
}

#endif
