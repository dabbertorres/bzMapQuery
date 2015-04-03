#ifndef MAP_SITE_HPP
#define MAP_SITE_HPP

#include <string>
#include <map>
#include <vector>
#include <utility>

#include <SFML/Network/Http.hpp>

namespace mq
{
	class MapSite
	{
		public:
			// format: <name, url>
			using MapPair = std::pair<std::string, std::string>;
			using StrMap = std::map<std::string, std::string>;
			using StrVec = std::vector<std::string>;

			StrMap getMaps(const std::string& cond = "");

			// given a url and a path to save the result to, attempts to download the map zip file.
			// returns the resulting filepath upon success. Otherwise, returns an empty string
			std::string downloadMap(const std::string& url, const std::string& savePath);

			bool isServerGood() const;

		protected:
			MapSite(const std::string& servUrl, const std::string& mapsUrl, const StrVec& pages);
			
			virtual MapPair getMap(const std::string& line, const std::string& dir) = 0;

			StrMap getMapsFromPage(const sf::Http::Response& resp, const std::string& dir, const std::string& cond);
			bool getPage(const std::string& url, sf::Http::Response& resp);

			bool sendRequest(const std::string& url, sf::Http::Response& resp);

		private:
			const std::string SERVER_URL;
			const std::string MAPS_URL;
			const StrVec PAGES;

			sf::Http server;
			bool serverGood;
	};
}

#endif // MAP_SITE_HPP