
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
	//auto it = std::find_if(v.begin(), v.end(), k.() != v.end())

	if (it != v.end())
		return true;

	return false;
}