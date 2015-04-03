#include "BattlezoneOneCom.hpp"

#include <sstream>

namespace mq
{
	BattlezoneOneCom::BattlezoneOneCom()
		: MapSite(	"www.battlezone1.com",
					"/downloads/maps/",
					{
						"DM/",
						"strat/",
						"strat_dm/",
						"MPI/",
						"IA/",
						"RO/",
					})
	{}

	MapSite::MapPair BattlezoneOneCom::getMap(const std::string& line, const std::string& dir)
	{
		MapPair pair;

		if(line.find("alt=\"[   ]\"") != std::string::npos)
		{
			// parse that shiznit
			std::size_t urlBegin = line.find("href=\"") + 6;
			std::size_t urlEnd = line.find('\"', urlBegin);
			std::size_t nameBegin = urlEnd + 2;
			std::size_t nameEnd = line.find("</a>", nameBegin);
			std::string key = line.substr(nameBegin, nameEnd - nameBegin);
			std::string val = dir + line.substr(urlBegin, urlEnd - urlBegin);
			key = key.substr(0, key.find("."));

			pair = {key, val};
		}

		return std::move(pair);
	}
}