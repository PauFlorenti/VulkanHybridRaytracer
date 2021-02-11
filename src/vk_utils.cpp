
#include "vk_utils.h"

template <typename T>
int vkutil::getIndex(std::vector<T> v, T k)
{
	auto it = std::find(v.begin(), v.end(), k);

	if (it != v.end())
	{
		return it - v.begin();
	}
	else
	{
		return -1;
	}
}

template <typename T>
bool vkutil::existsInVector(std::vector<T> v, T k)
{
	auto it = std::find(v.begin(), v.end(), k);

	if (it != v.end())
		return true;

	return false;
}

std::string vkutil::findFile(const std::string& filename, const std::vector<std::string>& directories, bool warn)
{
		std::ifstream stream;

	{
		stream.open(filename.c_str());
		if (stream.is_open())
			return filename;
	}

	for (const auto& directory : directories) 
	{
		std::string file = directory + "/" + filename;
		stream.open(file.c_str());
		if (stream.is_open())
			return file;
	}

	if (warn)
	{
		std::printf("File not found %s\n", filename.c_str());
		std::cout << "In directories: \n";
		for (const auto& dir : directories)
		{
			std::cout << dir.c_str() << std::endl;
		}
	}
	return {};
}
