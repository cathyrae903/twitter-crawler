#include "twitterCrawler.h"



const long long unsigned int START_USER_ID = 258604828;
const int MAX_FOLLOWERS = 16;
static int numSearcherThreads = 1;
static int numDownloaderThreads = 1;

static std::exception_ptr rateLimitDownloader = nullptr;
static std::exception_ptr rateLimitSearcher = nullptr;

std::mutex searchMutex;
std::mutex downloadMutex;

int main( int argc, char* argv[] )
{
    bool continueDownloader = true;
    bool continueSearcher = true;
    std::vector<std::thread> searcherThreads;
    std::vector<std::thread> downloaderThreads;
    std::vector<std::chrono::duration<int, std::micro>> searcherDurations;
    std::vector<std::chrono::duration<int, std::micro>> searcherAverageDurations;
    std::vector<std::chrono::duration<int, std::micro>> downloaderDurations;
    std::vector<std::chrono::duration<int, std::micro>> downloaderAverageDurations;
    std::chrono::duration<unsigned long long, std::micro> totalDuration = std::chrono::microseconds::zero();
    std::queue<unsigned long long> followers;       // Queue for followers that need to be crawled.
    std::vector<unsigned long long> crawledIds;     // IDs of users that have been crawled/searched?
    std::vector<unsigned long long> downloadIds;    // IDs of users whose profile pictures need to bd downloaded.
    std::vector<unsigned long long> downloadedIds;  // IDs of users whose profile pictures have been downloaded.         
    std::string replyMsg;                           // Stores the xml response
    
    // Authenticate
    twitCurl twitterObj;
    int response_code;
    response_code = authenticate(twitterObj);
    if (response_code != 0) 
        return response_code;

    // Display ratelimit status - sanity
    twitterObj.accountRateLimitGet();
    twitterObj.getLastWebResponse(replyMsg);
    printf("Rate limit status\n%s\n", replyMsg.c_str());

    // Get number of searcher and downloader threads from the command line.
    // If both values aren't present, they are assumed to be 1.
    try
    {
        if(argc == 3 && (std::stoi(argv[1]) >= 1 && std::stoi(argv[2]) >= 1))
        {
            numSearcherThreads = static_cast<int>(std::stoi(argv[1]));
            numDownloaderThreads = static_cast<int>(std::stoi(argv[2]));
            printf("[+] Arguments passed for number of threads. Using %d Searcher threads and %d Downloader threads.\n", numSearcherThreads, numDownloaderThreads);
        }
        else
        {
            throw std::invalid_argument("invalid_argument");
        }
    }
    catch(std::exception e)
    {
        printf("[-] Incorrect arguments passed. Using 1 Searcher thread and 1 Downloader thread.\n");
    }

    std::ofstream results;
    results.open("crawler_results.txt", std::ios::app);
    if(results.is_open())
    {
        results << "Results for " << numSearcherThreads << " searchers and " << numDownloaderThreads << " downloaders:\n";
        results.close();
    }
    
    twitCurl* clonedTwitterObj = twitterObj.clone(); // Clone the twitter object for use in the searcher and downloader threads.
    int followerCount = 0; // Prepare a stopping condition of MAX_FOLLOWERS.
    bool keepWaiting = true; // Flag for if downloaders can complete or need to wait for searcher to fill up data structure.
   
    // Start the search with one user's screen-name.
    followers.push(START_USER_ID);
    downloadIds.push_back(START_USER_ID);
    
    while (continueSearcher || continueDownloader)
    {
        // Start the timer for program execution.
        auto overall_start = high_resolution_clock::now();
        // Start the threads.
        if (continueSearcher)
        {
            for(int i = 0; i < numSearcherThreads; i++)
            {
                searcherThreads.push_back(std::thread(&startSearcher, 
                                        std::ref(*clonedTwitterObj),
                                        std::ref(followers), 
                                        std::ref(crawledIds),
                                        std::ref(downloadIds),
                                        std::ref(followerCount), 
                                        std::ref(searcherDurations)));
            }
        }
        if (continueDownloader)
        {
            for(int i = 0; i < numDownloaderThreads; i++)
            {
                downloaderThreads.push_back(std::thread(&startDownloader, 
                                            std::ref(twitterObj), 
                                            std::ref(downloadIds),
                                            std::ref(downloadedIds),
                                            std::ref(keepWaiting),
                                            std::ref(followerCount),
                                            std::ref(downloaderDurations)));
            }
        }

        // Reset
        continueSearcher = false;
        continueDownloader = false;
        
        // Wait for execution to finish and join all threads back to main.
        // There are two reasons for threads to join:
        //      1) They no longer have anything to do
        //      2) They have reached their rate limit
        std::vector<std::thread>::iterator itr;

        // Check all threads until none are remaining.
        for(itr=searcherThreads.begin(); itr < searcherThreads.end(); itr++)
        {
            if(itr->joinable())
            {
                // Join thread and remove it from looping structure if it is finished.
                itr->join();
                searcherThreads.erase(itr);
                itr--;
                printf("[+] Searcher thread joined.\n");
            }
        }
        for(itr=downloaderThreads.begin(); itr < downloaderThreads.end(); itr++)
        {
            if(itr->joinable())
            {
                // Join thread and remove it from looping structure if it is finished.
                itr->join();
                downloaderThreads.erase(itr);
                itr--;
                printf("[+] Downloader thread joined.\n");
            }
        }

        // Stop the timer for program execution.
        auto overall_stop = high_resolution_clock::now();

        // From here on, we will check to see if there is anymore work to be done

        // Check for rate limit exception for Searcher
        if(rateLimitSearcher) 
        {
            try
            {
                std::rethrow_exception(rateLimitSearcher);
            }
            catch (const std::exception &ex)
            {
                std::cout << "Searcher thread exited: " << ex.what() << std::endl;
                continueSearcher = true;
                rateLimitSearcher = nullptr;
            }
        }

        // Check for rate limit exception for Searcher
        if(rateLimitDownloader) 
        {
            try
            {
                std::rethrow_exception(rateLimitDownloader);
            }
            catch (const std::exception &ex)
            {
                std::cout << "Downloader thread exited: " << ex.what() << std::endl;
                continueDownloader = true;
                rateLimitDownloader = nullptr;
            }
        }

        // Calculate average duration for this rate limit window.
        std::vector<std::chrono::duration<int, std::micro>>::iterator itrDurations;
        std::chrono::duration<int, std::micro> sum = std::chrono::microseconds::zero();
        for(itrDurations=searcherDurations.begin(); itrDurations < searcherDurations.end(); itrDurations++)
        {
            sum += *itrDurations;
        }
        searcherAverageDurations.push_back(sum / static_cast<int>(numSearcherThreads));
        searcherDurations.clear();
        sum = std::chrono::microseconds::zero();
        for(itrDurations=downloaderDurations.begin(); itrDurations < downloaderDurations.end(); itrDurations++)
        {
            sum += *itrDurations;
        }
        downloaderAverageDurations.push_back(sum / static_cast<int>(numDownloaderThreads));
        downloaderDurations.clear();
        totalDuration += duration_cast<microseconds>(overall_stop - overall_start);

        std::cout << "All threads are currently joined.\n";
        std::cout << "Downloads remaining: " << downloadIds.size() << std::endl;
        std::cout << "Total users in queue: " << followers.size() << std::endl;
        std::cout << "Total followers crawled: " << followerCount << std::endl;
        if (continueSearcher || continueDownloader)
        {
            printf("More data to crunch, waiting...\n");
            std::time_t currTime = std::time(nullptr);
            std::cout << "Start time is " << std::asctime(std::localtime(&currTime));
            std::this_thread::sleep_for (std::chrono::minutes(15));
        }
    }

    // Save the results
    results.open("crawler_results.txt", std::ios::app);
    if(results.is_open())
    {
        results << "Users Searched: " << crawledIds.size() << "\n";

        // Remove duplicates
        std::sort(downloadedIds.begin(), downloadedIds.end());
        downloadedIds.erase(std::unique(downloadedIds.begin(), downloadedIds.end()), downloadedIds.end());
        results << "Images Downloaded: " << downloadedIds.size() << "\n";

        std::chrono::duration<int, std::micro> sum = std::chrono::microseconds::zero();
        for(int i = 0; i < searcherAverageDurations.size(); i++)
        {
            sum += searcherAverageDurations.at(i);
        }
        results << "Searcher: " << sum.count() << "\n";
        sum = std::chrono::microseconds::zero();
        for(int i = 0; i < downloaderAverageDurations.size(); i++)
        {
            sum += downloaderAverageDurations.at(i);
        }
        results << "Downloader: " << sum.count() << "\n";

        results << "Total: " << totalDuration.count() << "\n";

        results.close();
    }


    delete clonedTwitterObj;
    return 0;
}

int authenticate(twitCurl &twitterObj)
{
    // Get account credentials from file.
    std::ifstream credentials;
    std::string username("");
    std::string password("");

    credentials.open("cred.txt");

    std::getline(credentials, username);
    std::getline(credentials, password);

    credentials.close();

    /* OAuth flow begins */
    std::string tmpStr, tmpStr2;
    // Stores the JSON data returned from an API request
    std::string replyMsg;
    char tmpBuf[1024];

    // Set OAuth related params.
    std::ifstream consumerKeyIn;
    std::ifstream consumerKeySecretIn;

    // Step 1.a: Retrieve consumer keys from file
    consumerKeyIn.open( "consumer_token_key.txt" );
    consumerKeySecretIn.open( "consumer_token_secret.txt" );

    memset( tmpBuf, 0, 1024 );
    consumerKeyIn >> tmpBuf;
    twitterObj.getOAuth().setConsumerKey(std::string(tmpBuf));

    memset( tmpBuf, 0, 1024 );
    consumerKeySecretIn >> tmpBuf;
    twitterObj.getOAuth().setConsumerSecret(std::string(tmpBuf));

    consumerKeyIn.close();
    consumerKeySecretIn.close();

    // Step 1.b: Retrieve OAuth access token from files
    std::string myOAuthAccessTokenKey("");
    std::string myOAuthAccessTokenSecret("");
    std::ifstream oAuthTokenKeyIn;
    std::ifstream oAuthTokenSecretIn;

    oAuthTokenKeyIn.open( "oauth_token_key.txt" );
    oAuthTokenSecretIn.open( "oauth_token_secret.txt" );

    memset( tmpBuf, 0, 1024 );
    oAuthTokenKeyIn >> tmpBuf;
    myOAuthAccessTokenKey = tmpBuf;

    memset( tmpBuf, 0, 1024 );
    oAuthTokenSecretIn >> tmpBuf;
    myOAuthAccessTokenSecret = tmpBuf;

    oAuthTokenKeyIn.close();
    oAuthTokenSecretIn.close();

    if( myOAuthAccessTokenKey.size() && myOAuthAccessTokenSecret.size() )
    {
        // If we already have these keys, then no need to go through auth again
        printf("[+] Found OAuth Access Token\n");
        //printf( "\nUsing:\nKey: %s\nSecret: %s\n\n", myOAuthAccessTokenKey.c_str(), myOAuthAccessTokenSecret.c_str() );

        twitterObj.getOAuth().setOAuthTokenKey( myOAuthAccessTokenKey );
        twitterObj.getOAuth().setOAuthTokenSecret( myOAuthAccessTokenSecret );
    }
    else
    {
        printf("[-] OAuth failed.\n");
        return 511;
    }
    /* OAuth flow ends */

    // Account credentials verification
    if( twitterObj.accountVerifyCredGet() ) {
        twitterObj.getLastWebResponse( replyMsg );
        printf("[+] Account Credential Verification Successful.\n");
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf("\n[-] Account Credential Verification Unsuccessful!\n");
        printf("\n[-] accountVerifyCredGet error:\n%s\n", replyMsg.c_str());
    }
    return 0;
}

// Downloader
// Download the profile pictures for users whose ID is in downloadIds.
void startDownloader(twitCurl &twitterObj,
                     std::vector<long long unsigned int> &downloadIds,
                     std::vector<long long unsigned int> &downloadedIds,
                     const bool &keepWaiting, 
                     int &followerCount,
                     std::vector<std::chrono::duration<int, std::micro>> &downloaderDurations)
{
    std::string userId;
    std::string replyMsg;

    printf("[+] Starting downloader thread\n");
    // Start this thread's timer.
    auto start = high_resolution_clock::now();

    try 
    {
        while(followerCount < MAX_FOLLOWERS || !downloadIds.empty())
        {
            // Begin critical section.
            downloadMutex.lock();
            if(!downloadIds.empty() && !std::binary_search(downloadedIds.begin(), downloadedIds.end(), *(downloadIds.begin())))
            {
                userId = std::to_string(*(downloadIds.begin()));

                downloadIds.erase(downloadIds.begin());

                // Get user information.
                if(twitterObj.userGet(userId, true))
                {
                    twitterObj.getLastWebResponse(replyMsg);
                }
                else
                {
                    twitterObj.getLastCurlError(replyMsg);
                    printf("\nuserGet error:\n%s\n", replyMsg.c_str());
                }
                if (replyMsg.substr(2, 6) == "errors") 
                {
                    printf("[-] Hit the rate limit in a downloader, exiting...\n");
                    printf("%s\n", replyMsg.c_str());
                    downloadIds.push_back(strtoull(userId.c_str(), nullptr, 10));
                    throw std::runtime_error("downloaderRateLimit");
                }

                // Mark this user as having their profile image downloaded.
                downloadedIds.push_back(strtoull(userId.c_str(), nullptr, 10));
            }
            else
            {
                userId = "";
            }

            downloadMutex.unlock();
            // End critical section.

            // Extract profile_image_url_https from replyMsg and download it.
            if(!userId.empty())
            {
                std::string screenName = parseJSONScreenName(replyMsg);
                printf("[+] Grabbing profile picture of %s...\n", screenName.c_str());
                if(system(("curl " + parsePFPUrl(replyMsg) + " -s --create-dirs -o ./images/" + screenName + ".jpg ").c_str()))
                    // Note that if there is a problem, the user ID is still in downloadedIds.
                    printf("[-] Encountered problem downloading profile picture.\n");
            }
        }
    }
    catch (...) 
    {   
        rateLimitDownloader = std::current_exception();
        // If the rate limit was hit, we still need to unlock the mutex.
        downloadMutex.unlock();
    }

    // Stop this thread's timer.
    auto stop = high_resolution_clock::now();
    downloaderDurations.push_back(duration_cast<microseconds>(stop - start));

    std::cout << "[+] Ending downloader thread. Total thread time " << duration_cast<microseconds>(stop - start).count() << ".\n";

    return;
}


// Searcher
// Search the twitter accounts to retrieve the followers' IDs.
void startSearcher(twitCurl &twitterObj, 
                    std::queue<long long unsigned int> &followers,
                    std::vector<long long unsigned int> &crawledIds,
                    std::vector<long long unsigned int> &downloadIds,
                    int &followerCount,
                    std::vector<std::chrono::duration<int, std::micro>> &searcherDurations)
{
    std::string replyMsg;
    long long unsigned int userId;
    std::string nextCursor = "-1";

    printf("[+] Start searcher thread\n");

    // Start this thread's timer.
    auto start = high_resolution_clock::now();

    try 
    {
        while(followerCount < MAX_FOLLOWERS)
        {
            nextCursor = "-1";

            // Begin critical section.
            searchMutex.lock();
            // If there is a follower, grab em off the vector
            if(!followers.empty())
            {
                userId = followers.front();
                // Remove this user from the queue.
                followers.pop();
            }
            else
            {
                userId = 0;
            }

            // Only examine this user's followers if this user has not already been crawled.
            if(userId && !std::binary_search(crawledIds.begin(), crawledIds.end(), userId))
            {
                //printf("[+] Crawling user %i...\n", userId);
                std::cout << "[+] Crawling user " << userId << std::endl;
                // Increase the depth of the "searcher".
                followerCount++;

                // Get this user's followers' IDs.
                while (nextCursor != "0" ) 
                {
                    printf("[+] Cursor iteration: %s\n", nextCursor.c_str());
                    if(twitterObj.followersIdsGet(nextCursor, std::to_string(userId), true))
                    {
                        // Get the response and put it in replyMsg
                        twitterObj.getLastWebResponse(replyMsg);
                        if (replyMsg.substr(2, 6) == "errors") 
                        {
                            printf("[-] Hit the rate limit in a searcher, exiting...\n");
                            printf("%s\n", replyMsg.c_str());
                            throw std::runtime_error("searcherRateLimit");
                        }

                        // Grab the next cursor if there is one
                        nextCursor = parseNextCursor(replyMsg);
                        printf("[+] Adding followers to queue\n");

                        // Extract follower IDs from web response.
                        std::vector<std::string> followersIds = parseJSONList(replyMsg);
                        for(int i = 0; i < followersIds.size(); i++)
                        {
                            downloadIds.push_back(strtoull(followersIds[i].c_str(), nullptr, 10)); // base 10
                            followers.push(strtoull(followersIds[i].c_str(), nullptr, 10));
                        }

                        // Mark this user as crawled.
                        crawledIds.push_back(userId);

                        // Sort the vector in ascending order so that search is quicker.
                        std::sort(crawledIds.begin(), crawledIds.end());
                    }
                    else 
                    {
                        twitterObj.getLastCurlError(replyMsg);
                        printf( "\n[-] followersIdsGet error:\n%s\n", replyMsg.c_str() );
                        break;
                    }    
                }
            }
            searchMutex.unlock();
            // End critical section.
        }
    }
    catch (...)
    {
        rateLimitSearcher = std::current_exception();
        // If the rate limit was hit, we still need to unlock the mutex.
        searchMutex.unlock();
    }

    // Stop this thread's timer.
    auto stop = high_resolution_clock::now();
    searcherDurations.push_back(duration_cast<microseconds>(stop - start));

    std::cout << "[+] Ending searcher thread. Total thread time " << duration_cast<microseconds>(stop - start).count() << ".\n";

    return;
}


// Takes a set of JSON data from the twitter response and parses out the pfp url for download
std::string parsePFPUrl(std::string xml_data) {
    std::regex rePFP ("profile_image_url\":\"(.+?(?=\"))"); // To find a profile picture url
    std::smatch match;
    std::string url; 

    // Pull the url out of the xml_data
    std::regex_search(xml_data, match, rePFP);
    url = match.str(1);
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '\\') {
            url.erase(i, 1);
            i--;
        }
    }
    return url;
}

// Takes JSON data from the twitter response and parses out IDs
std::vector<std::string> parseJSONList(std::string xml_data)//, std::vector<std::string> &ids)
{
    std::vector<std::string> ids;
    std::regex reIdList("\\[[\\d, ]*\\]");
    std::regex reIds("(\\d{5,})");
    std::smatch matches;
    std::string idList;

    // Pull the objects out of the xml_data
    if(std::regex_search(xml_data, matches, reIdList))
    {
        // Grab the list part of the string for easier parsing.
        idList = matches[0].str();

        // Grab each group that matches the regex.
        std::sregex_iterator itr(idList.begin(), idList.end(), reIds);
        std::sregex_iterator end;
        while(itr != end)
        {
            std::smatch match = *itr;
            ids.push_back(match.str());
            itr++;
        }
    }

    return ids;
}

// Takes JSON data from the twitter response and parses out the screen name.
std::string parseJSONScreenName(std::string xml_data)
{
    std::regex reScreenName("\"screen_name\":\"([^\"]*)\"");
    std::smatch match;

    // Pull the information out of the xml_data
    if(std::regex_search(xml_data, match, reScreenName))
        // Grab the screen name in group 1
         return match[1].str();
    return "";
}

std::string parseNextCursor(std::string xml_data) 
{
    std::regex reCursor("\"next_cursor_str\":\"([^\"]*)\"");
    std::smatch match;
    // Pull the information out of the xml_data
    if(std::regex_search(xml_data, match, reCursor)) {
        // Grab the screen name in group 1
         return match.str(1);
    }
    return "";
}

