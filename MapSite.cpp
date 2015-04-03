#include "MapSite.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

namespace mq
{
	MapSite::StrMap MapSite::getMaps(const std::string& cond)
	{
		StrMap maps;

		for(auto& page : PAGES)
		{
			sf::Http::Response resp;

			bool result = getPage(MAPS_URL + page, resp);

			if(result)
			{
				StrMap pageMaps = getMapsFromPage(resp, page, cond);

				for(auto& map : pageMaps)
				{
					maps[map.first] = map.second;
				}
			}
		}

		return std::move(maps);
	}

	std::string MapSite::downloadMap(const std::string& url, const std::string& savePath)
	{
		sf::Http::Response response;
		bool result = getPage(MAPS_URL + url, response);

		if(!result)
			return "";

		std::string fileName = savePath + url.substr(url.find_last_of("/") + 1);

		std::ofstream fout;
		fout.open(fileName, std::ofstream::binary);

		if(fout.bad())
		{
			std::cout << "[ERROR]: Could not open file: " << fileName << '\n';
			return "";
		}

		fout.write(response.getBody().data(), response.getBody().size());

		fout.close();

		return fileName;
	}

	bool MapSite::isServerGood() const
	{
		return serverGood;
	}

	MapSite::MapSite(const std::string& servUrl, const std::string& mapsUrl, const StrVec& pages)
		:	SERVER_URL(servUrl),
			MAPS_URL(mapsUrl),
			PAGES(pages),
			server(servUrl),
			serverGood(false)
	{
		sf::Http::Response resp = server.sendRequest({});	// sends default request, uri: "/", method: GET, body: ""

		serverGood = resp.getStatus() == sf::Http::Response::Status::Ok;
	}

	MapSite::StrMap MapSite::getMapsFromPage(const sf::Http::Response& resp, const std::string& dir, const std::string& cond)
	{
		StrMap maps;

		std::stringstream ss(resp.getBody());

		std::string mapLine;
		while(std::getline(ss, mapLine))
		{
			MapPair pair = getMap(mapLine, dir);

			if(!pair.first.empty() && !pair.second.empty())
				if(!cond.empty() && pair.first.find(cond) != std::string::npos)
					maps[pair.first] = pair.second;
		}

		return std::move(maps);
	}

	bool MapSite::getPage(const std::string& url, sf::Http::Response& resp)
	{
		sf::Http::Request req(url, sf::Http::Request::Method::Get);
		resp = server.sendRequest(req);
		sf::Http::Response::Status status = resp.getStatus();

		if(status != sf::Http::Response::Ok)
		{
			std::cout << "[ERROR]: Bad response from server: " << status << " on request for: " << url << '\n';
			return false;
		}

		return true;
	}

	bool MapSite::sendRequest(const std::string& url, sf::Http::Response& resp)
	{
		if(serverGood)
		{
			resp = server.sendRequest({url});

			return resp.getStatus() == sf::Http::Response::Status::Ok;
		}
		else
		{
			return false;
		}
	}
}