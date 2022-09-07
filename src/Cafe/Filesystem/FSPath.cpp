#include "FSPath.h"

#ifdef BOOST_OS_UNIX

FSPath& FSPath::operator/ (const FSPath & other)
{
	*this /= other;
	return *this;
}

FSPath& FSPath::operator/= (const FSPath & other)
{
	// todo: implement caching.

	// explore directories recursively and find the matching cases.
	fs::path relPath = other.relative_path();
	fs::path correctedPath = empty() ? other.root_path() : *this;

	// helper function to convert a path's alphabet characters to lowercase.
	auto static lowercase_path = [](FSPath const & path)
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
