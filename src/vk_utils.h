#pragma once

#include "vk_types.h"

namespace vkutil
{
	template <typename T>
	int getIndex(std::vector<T> v, T k);

	template <typename T>
	bool existsInVector(std::vector<T> v, T k);
}