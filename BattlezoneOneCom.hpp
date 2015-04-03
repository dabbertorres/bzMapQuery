#ifndef BATTLEZONE_ONE_COM_HPP
#define BATTLEZONE_ONE_COM_HPP

#include "MapSite.hpp"

namespace mq
{
	class BattlezoneOneCom : public MapSite
	{
		public:
			BattlezoneOneCom();

		protected:
			virtual MapPair getMap(const std::string& line, const std::string& dir);
	};
}

#endif // BATTLEZONE_ONE_COM_HPP