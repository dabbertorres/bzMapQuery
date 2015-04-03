// stl
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

// sfml
#include <SFML/Network/Http.hpp>

// libzip
#include <zip.h>

// os
#ifdef __linux__
	#include <unistd.h>
	#include <sys/stat.h>
#else
	#include <windows.h>
#endif

// servers
#include "BattlezoneOneCom.hpp"

using bbyte = char;

/* constants */
#ifdef __linux__
	constexpr unsigned int PATH_MAX = 1024;
#else
	const unsigned int PATH_MAX = 1024;	// friggin VS not having friggin constexpr is friggin dumb
#endif

/* functions */
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
	mq::MapSite::StrVec args(argv + 1, argv + argc);	// "+ 1" to skip first arg. It's the name of the program

	if(args.size() < 1)
	{
		std::cout << "[ERROR]: Not enough arguments. Type at least something to search for after the exe name.\n";
		return 3;
	}

	std::string searchStr = args[0];

	for(auto i = 1u; i < args.size(); i++)
		searchStr.append(" " + args[i]);

	mq::BattlezoneOneCom bz1server;

	if(!bz1server.isServerGood())
	{
		std::cout << "[ERROR]: Cannot contact server. Exiting.\n";
		return 1;
	}

	mq::MapSite::StrMap results = bz1server.getMaps(searchStr);

	if(results.empty())
	{
		std::cout << "No results found for \"" << searchStr << "\"!\n";
		return 0;
	}

	for(auto it = results.begin(); it != results.end(); it++)
	{
		std::cout << std::distance(results.begin(), it) << ") " << it->first << " : " << it->second << '\n';
	}

	bool badInput = true;
	std::string input;
	std::vector<unsigned int> choices(1);

	while(badInput)
	{
		std::cout << "\nEnter the number(s) of the map to download (\"-1\" to cancel): ";
		std::getline(std::cin, input);

		if(input == "-1")
			return 0;

		std::stringstream ss(input);

		badInput = false;
		try
		{
			while(ss >> choices.back())
				choices.emplace_back();

			choices.erase(--choices.end());	// get rid of extra item

			for(auto& c : choices)
				if(c >= results.size())
					throw 0;
		}
		catch(...)
		{
			badInput = true;
			choices.resize(1);
			std::cout << "[ERROR]: Bad input. Try again!\n";
		}
	}

	for(auto& c : choices)
	{
		auto choiceIter = std::next(results.begin(), c);
		std::string resFileName = bz1server.downloadMap(choiceIter->second, "./");

		if(resFileName.empty())
		{
			std::cout << "[ERROR]: Download failed for \"" << choiceIter->first << "\". Exiting.\n";
			return 2;
		}

		extractMap(resFileName);
	}

	return 0;
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
	folderName = folderName.substr(0, folderName.find_last_of('.')) + "/";	// cut off file extension, add dir char

	stripWebChars(folderName);

	if(!makeFolder(folderName))
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

			#ifndef __linux__
				#pragma warning(push)
				#pragma warning(disable: 4244)
			#endif
			std::vector<bbyte> bytes(fileInfo.size);	// just gotta deal with this conversion
			#ifndef __linux__
				#pragma warning(pop)
			#endif

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
		if(CreateDirectory(name.c_str(), NULL) != 0)	// success check
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
	auto pos = str.find("%20");		// look for annoying stuff in string
	while(pos != std::string::npos)
	{
		str.replace(pos, 3, " ");	// replace it with a space
		pos = str.find("%20");
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
