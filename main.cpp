// stl
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <map>
#include <vector>

// sfml
#include <SFML/Network/Http.hpp>
#include <SFML/Network/Ftp.hpp>

// zip
#include <zip.h>

// os
#ifdef __linux__
	#include <unistd.h>
	#include <sys/stat.h>
#else
	#include <windows.h>
#endif

/* types */
enum class MapType
{
	DM,
	ST,
	ST_DM,
	MPI,
	IA,
	RO,
	All,
};

using byte = char;
using StrVec = std::vector<std::string>;
using StrMap = std::map<std::string, std::string>;

/* constants */
constexpr unsigned int PATH_MAX = 1024;
const std::string SERVER_URL = "www.battlezone1.com";
const std::string MAPS_PATH = "/downloads/maps/";

const std::map<MapType, std::string> MAP_TYPE_PATHS =	{
															{MapType::DM, "DM/"},
															{MapType::ST, "strat/"},
															{MapType::ST_DM, "strat_dm/"},
															{MapType::MPI, "MPI/"},
															{MapType::IA, "IA/"},
															{MapType::RO, "RO/"}
														};

/* functions */
// given a request, attempts to request page from server, and set given Response arg to result
bool getPage(sf::Http&, const std::string&, sf::Http::Request::Method, sf::Http::Response&);

// returns a map of map names and respective urls
StrMap getMapsFromPage(const sf::Http::Response&, const std::string&);

// searches the BZ server's maps for a given string, optionally constrained to a map type
// and returns a map of names and urls
StrMap findMatches(const std::string&, sf::Http&, MapType = MapType::All);

// attempts to download map via ftp
bool downloadMap(const std::string&, sf::Http& server);

// attempts to extract map files from downloaded zip
bool extractMap(const std::string&);

// adds a map to BZ's netmis file
bool addMapToNetmis(const std::string&);

// create a folder given a path
bool makeFolder(const std::string&);

// strip out web characters from string ex: '%20'
void stripWebChars(std::string&);

// get path of our program
std::string getExePath();

// get install location of Battlezone
std::string getBZinstallDir();

int main(int argc, char** argv)
{
	StrVec args(argv + 1, argv + argc);	// "+ 1" to skip first arg. It's the name of the program

	if(args.size() < 1)
	{
		std::cout << "Not enough arguments. Type at least something to search for after the exe name.\n";
		return 3;
	}

	std::string searchStr = args[0];

	for(auto i = 1u; i < args.size(); i++)
		searchStr.append(" " + args[i]);

	sf::Http bzServer(SERVER_URL);
	sf::Http::Response testResp;

	bool serverTest = getPage(bzServer, MAPS_PATH, sf::Http::Request::Head, testResp);

	if(!serverTest)
	{
		std::cout << "Cannot contact server. Exiting.\n";
		return 1;
	}

	StrMap results = findMatches(searchStr, bzServer);

	for(auto it = results.begin(); it != results.end(); it++)
	{
		std::cout << std::distance(results.begin(), it) << ") " << it->first << " : " << it->second << '\n';
	}

	bool badInput = true;
	std::string input;
	unsigned int choice;

	while(badInput)
	{
		std::cout << "\nEnter the number of the map to download (\"-1\" to cancel): ";
		std::cin >> input;

		if(input == "-1")
			return 0;

		badInput = false;
		try
		{
			choice = std::stoul(input);

			if(choice >= results.size())
				throw 0;
		}
		catch(...)
		{
			badInput = true;
		}
	}

	auto choiceIter = std::next(results.begin(), choice);
	bool dlRes = downloadMap(MAPS_PATH + choiceIter->second, bzServer);

	if(!dlRes)
	{
		std::cout << "[ERROR]: Download failed. Exiting.\n";
		return 2;
	}

	return 0;
}

bool getPage(sf::Http& server, const std::string& uri, sf::Http::Request::Method method, sf::Http::Response& resp)
{
	sf::Http::Request req(uri, method);
	resp = server.sendRequest(req);
	sf::Http::Response::Status status = resp.getStatus();

	if(status != sf::Http::Response::Ok)
	{
		std::cout << "[ERROR]: Bad response from server: " << status << " on request for: " << uri << '\n';
		return false;
	}

	return true;
}

StrMap getMapsFromPage(const sf::Http::Response& resp, const std::string& dir)
{
	StrMap maps;

	std::stringstream ss(resp.getBody());

	std::string mapLine;
	while(std::getline(ss, mapLine))
	{
		// found a map line
		if(mapLine.find("alt=\"[   ]\"") != std::string::npos)
		{
			std::size_t urlBegin = mapLine.find("href=\"") + 6;
			std::size_t urlEnd = mapLine.find('\"', urlBegin);
			std::size_t nameBegin = urlEnd + 2;
			std::size_t nameEnd = mapLine.find("</a>", nameBegin);
			std::string key = mapLine.substr(nameBegin, nameEnd - nameBegin);
			std::string val = dir + mapLine.substr(urlBegin, urlEnd - urlBegin);
			key = key.substr(0, key.find("."));
			maps[key] = val;
		}
	}

	return std::move(maps);
}

StrMap findMatches(const std::string& find, sf::Http& server, MapType mapType)
{
	StrMap matches;

	if(mapType != MapType::All)	// search a single map page
	{
		sf::Http::Response response;
		bool result = getPage(server, MAPS_PATH + MAP_TYPE_PATHS.at(mapType), sf::Http::Request::Get, response);

		if(!result)
			return {};

		StrMap results = getMapsFromPage(response, MAP_TYPE_PATHS.at(mapType));

		for(auto& r : results)
		{
			if(r.first.find(find) != std::string::npos)
				matches[r.first] = r.second;
		}
	}
	else	// search all map pages
	{
		for(auto& path : MAP_TYPE_PATHS)
		{
			sf::Http::Response response;
			bool result = getPage(server, MAPS_PATH + path.second, sf::Http::Request::Get, response);

			if(result)
			{
				StrMap results = getMapsFromPage(response, path.second);

				for(auto& r : results)
				{
					if(r.first.find(find) != std::string::npos)
						matches[r.first] = r.second;
				}
			}
		}
	}

	return std::move(matches);
}

bool downloadMap(const std::string& url, sf::Http& server)
{
	sf::Http::Response response;
	bool result = getPage(server, url, sf::Http::Request::Get, response);

	if(!result)
		return false;

	std::string fileName = url.substr(url.find_last_of("/") + 1);

	std::ofstream fout;
	fout.open(fileName);

	if(fout.bad())
	{
		std::cout << "[ERROR]: Could not open file: " << fileName << '\n';
		return false;
	}

	fout << response.getBody();

	fout.close();

	return extractMap(fileName);
}

bool extractMap(const std::string& file)
{
	int err = 0;
	zip* zipFile = zip_open(file.c_str(), 0, &err);

	if(!zipFile)
	{
		std::cout << "[ERROR]: Failed to open archive file: " << file << ".\n";
		return false;
	}

	auto fileNumber = zip_get_num_entries(zipFile, 0);

	std::string folderName = file.substr(file.find_last_of('/') + 1);	// cut off preceding directories in path
	folderName = folderName.substr(0, file.find_last_of('.')) + "/";	// cut off file extension, add dir char

	stripWebChars(folderName);

	if(!makeFolder(getExePath() + folderName))
		return false;

	for(auto i = 0u; i < fileNumber; i++)
	{
		zip_file* zipped = zip_fopen_index(zipFile, i, 0);
		struct zip_stat fileInfo;
		zip_stat_init(&fileInfo);
		zip_stat_index(zipFile, i, 0, &fileInfo);

		if(fileInfo.valid & ZIP_STAT_NAME && fileInfo.valid & ZIP_STAT_SIZE && fileInfo.valid & ZIP_STAT_COMP_SIZE)
		{
			std::string fileStr = fileInfo.name;
			if(fileStr.find('.') == std::string::npos)	// if we don't have a dot, this is a folder
			{
				continue;	// skip this folder
			}

			if(fileStr.find('/') != std::string::npos)	// if we have any dir chars in the string, strip out dirs
			{
				fileStr = fileStr.substr(fileStr.find_last_of('/') + 1);
			}

			std::vector<byte> bytes(fileInfo.size);

			zip_fread(zipped, bytes.data(), fileInfo.size);

			std::ofstream fout;

			fout.open(folderName + fileStr, std::ofstream::binary);

			if(fout.bad())
			{
				std::cout << "[ERROR]: Unable to extract file: " << fileInfo.name << '\n';
				return false;
			}

			fout.write(bytes.data(), bytes.size());

			fout.close();
		}
		else
		{
			std::cout << "[ERROR]: Bad file data for file in archive: " << file << '\n';
			return false;
		}

		zip_fclose(zipped);
	}

	zip_close(zipFile);

	return true;
}

bool addMapToNetmis(const std::string& map)
{
	return true;
}

bool makeFolder(const std::string& name)
{
	#ifdef __linux__
		struct stat st;
		int status = 0;

		if(stat(name.c_str(), &st) != 0)	// check if folder already exists
		{
			if(mkdir(name.c_str(), 0700) != 0)	// check for failure
			{
				std::cout << "[ERROR]: Failure creating folder: " << name << '\n';
				return false;
			}
		}
	#else
		if(CreateDirectory(name.c_str(), NULL) == 0)	// success check
		{
			// do nothing.
		}
	#endif
	else
	{
		std::cout << "[ERROR]: Folder already exists: " << name << '\n';
		return false;
	}

	return true;
}

std::string getExePath()
{
	std::string path;
	char buffer[PATH_MAX];

	#ifdef __linux__
		readlink("/proc/self/exe", buffer, PATH_MAX);
	#else
		GetModuleFileName(NULL, buffer, PATH_MAX);
	#endif

	path = buffer;
	path = path.substr(0, path.find_last_of('/') + 1);

	return std::move(path);
}

void stripWebChars(std::string& str)
{
	if(str.find("%20") != std::string::npos)	// look for annoying stuff in string
	{
		str.replace(str.find("%20"), 3, " ");	// replace it with a space
	}
}

std::string getBZinstallDir()
{
	std::string path;

	#ifdef __linux__

	#else

	#endif

	return path;
}
