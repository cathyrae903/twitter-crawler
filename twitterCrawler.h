#include <bits/stdc++.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <stdlib.h>
#include <thread>
#include <vector>
#include "include/twitcurl.h"

using namespace std::chrono;


int authenticate(twitCurl &twitterObj);

void startDownloader(twitCurl &twitterObj,
                     std::vector<long long unsigned int> &downloadIds, 
                     std::vector<long long unsigned int> &downloadedIds,
                     const bool &keepWaiting,
                     int &followerCount,
                     std::vector<std::chrono::duration<unsigned long long, std::micro>> &downloaderDurations);

void startSearcher(twitCurl &twitterObj,
                    std::queue<long long unsigned int> &followers,
                    std::vector<long long unsigned int> &crawledIds,
                    std::vector<long long unsigned int> &downloadIds,
                    int &followerCount,
                    std::vector<std::chrono::duration<unsigned long long, std::micro>> &searcherDurations);

std::vector<std::string> parseJSONList(std::string xml_data);

std::string parsePFPUrl(std::string xml_data);

std::string parseJSONScreenName(std::string xml_data);

std::string parseNextCursor(std::string xml_data);