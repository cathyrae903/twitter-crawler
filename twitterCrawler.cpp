#include <regex>
#include <iostream>
#include <stdlib.h>
#include "twitterCrawler.h"


void startDownloader(twitCurl &twitterObj, std::vector<long long unsigned int> &downloadIds, const bool &keepWaiting);
void startSearcher(twitCurl &twitterObj, std::queue<long long unsigned int> &followers,
                    std::vector<long long unsigned int> &crawledIds,
                    std::vector<long long unsigned int> &downloadIds,
                    int &followerCount, std::string &nextCursor);
std::string parse_pfp_url(std::string xml_data);
std::vector<std::string> parseJSONList(std::string xml_data);
std::string parseJSONScreenName(std::string xml_data);

std::mutex searchMutex;
std::mutex downloadMutex;

const int MAX_FOLLOWERS = 1;
static std::string nextCursor("");

int main( int argc, char* argv[] )
{
    std::vector<std::thread> searcherThreads;
    std::vector<std::thread> downloaderThreads;
    std::queue<unsigned long long> followers;       // Queue for followers that need to be crawled.
    std::vector<unsigned long long> crawledIds;     // IDs of users that have been crawled/searched?
    std::vector<unsigned long long> downloadIds;    // IDs of users whose profile pictures need to bd downloaded.
    std::vector<unsigned long long> downloadedIds;  // IDs of users whose profile pictures have been downloaded.
    std::string twitter_response;                   // Stores the xml response

    // Get account credentials from file.
    std::ifstream credentials;
    std::string username("");
    std::string password("");

    credentials.open("cred.txt");

    std::getline(credentials, username);
    std::getline(credentials, password);

    credentials.close();

    /* OAuth flow begins */
    twitCurl twitterObj;
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
        //printf("\ntwitterClient:: twitCurl::accountVerifyCredGet web response:\n%s\n", replyMsg.c_str());
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf("\n[-] Account Credential Verification Unsuccessful!\n");
        printf("\n[-] twitterClient:: twitCurl::accountVerifyCredGet error:\n%s\n", replyMsg.c_str());
    }

    //std::string start_user = "cheetah1704";     // Username to start search at
    //std::string url;


    twitCurl* clonedTwitterObj = twitterObj.clone();
    // Prepare a stopping condition of MAX_FOLLOWERS.
    int followerCount = 0;
    // Flag for if downloaders can complete or need to wait for searcher to fill up data structure.
    bool keepWaiting = true;
    // Start the search with one user's screen-name.
    followers.push(258604828);
    searcherThreads.push_back(std::thread(&startSearcher, std::ref(*clonedTwitterObj),
                              std::ref(followers), std::ref(crawledIds), std::ref(downloadIds), std::ref(followerCount), std::ref(nextCursor)));
    searcherThreads.push_back(std::thread(&startSearcher, std::ref(*clonedTwitterObj),
                              std::ref(followers), std::ref(crawledIds), std::ref(downloadIds), std::ref(followerCount), std::ref(nextCursor)));

    downloaderThreads.push_back(std::thread(&startDownloader, std::ref(twitterObj), std::ref(downloadIds), std::ref(keepWaiting)));
    downloaderThreads.push_back(std::thread(&startDownloader, std::ref(twitterObj), std::ref(downloadIds), std::ref(keepWaiting)));
    downloaderThreads.push_back(std::thread(&startDownloader, std::ref(twitterObj), std::ref(downloadIds), std::ref(keepWaiting)));
    downloaderThreads.push_back(std::thread(&startDownloader, std::ref(twitterObj), std::ref(downloadIds), std::ref(keepWaiting)));

    // Wait for execution to finish and join all threads back to main.
    std::vector<std::thread>::iterator itr;
    // Check all threads until none are remaining.
    while(searcherThreads.size() > 0)
    {
        for(itr=searcherThreads.begin(); itr < searcherThreads.end(); itr++)
        {
            if((*itr).joinable())
            {
                // Join thread and remove it from looping structure if it is finished.
                (*itr).join();
                searcherThreads.erase(itr);
                itr--;
            }
        }
    }
    keepWaiting = false;
    while(downloaderThreads.size() > 0)
    {
        for(itr=downloaderThreads.begin(); itr < downloaderThreads.end(); itr++)
        {
            if((*itr).joinable())
            {
                // Join thread and remove it from looping structure if it is finished.
                (*itr).join();
                downloaderThreads.erase(itr);
                itr--;
            }
        }
    }

    return 0;
}


// Downloader
// Download the profile pictures for users whose ID is in downloadIds.
void startDownloader(twitCurl &twitterObj, std::vector<long long unsigned int> &downloadIds, const bool &keepWaiting)
{
    std::string userId;
    std::string replyMsg;

    while(keepWaiting || !downloadIds.empty())
    {
        // Begin critical section.
        downloadMutex.lock();
        if(!downloadIds.empty())
        {
            userId = std::to_string(*downloadIds.begin());

            downloadIds.erase(downloadIds.begin());

            // Get user information.
            //nextCursor = "";
            if(twitterObj.userGet(userId, true))
            {
                twitterObj.getLastWebResponse(replyMsg);
            }
            else
            {
                twitterObj.getLastCurlError(replyMsg);
                printf("\ntwitterClient:: twitCurl::userGet error:\n%s\n", replyMsg.c_str());
            }
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
            printf("[+] Grabbing pfp...\n");
            system(("curl " + parse_pfp_url(replyMsg) + " -s --create-dirs -o ./images/" + parseJSONScreenName(replyMsg) + ".jpg ").c_str());
        }
    }
    return;
}


// Searcher
void startSearcher(twitCurl &twitterObj, std::queue<long long unsigned int> &followers,
                    std::vector<long long unsigned int> &crawledIds,
                    std::vector<long long unsigned int> &downloadIds,
                    int &followerCount, std::string &nextCursor)
{
    std::string replyMsg;
    long long unsigned int userId;

    while(!followers.empty() && followerCount < MAX_FOLLOWERS)
    {
        // Begin critical section.
        searchMutex.lock();
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
        searchMutex.unlock();
        // End critical section.

        // Only examine this user's followers if this user has not already been crawled.
        if(userId && !std::binary_search(crawledIds.begin(), crawledIds.end(), userId))
        {
            printf("Crawling user %u...\n", userId);
            // Increase the depth of the "searcher".
            followerCount++;

            // Begin critical section.
            searchMutex.lock();
            // Get this user's followers' IDs.
            if(twitterObj.followersIdsGet(nextCursor, std::to_string(userId), true))
            {
                twitterObj.getLastWebResponse(replyMsg);
                printf( "twitterClient:: twitCurl::followersIdsGet for user [%u] web response:\n%s\n",
                        userId, replyMsg.c_str() );

                nextCursor = "";
                size_t nNextCursorStart = replyMsg.find("next_cursor");
                if( std::string::npos == nNextCursorStart )
                {
                    nNextCursorStart += strlen("next_cursor:\"");
                    size_t nNextCursorEnd = replyMsg.substr(nNextCursorStart).find(",");
                    if( std::string::npos != nNextCursorEnd )
                    {
                        nextCursor = replyMsg.substr(nNextCursorStart, (nNextCursorEnd - nNextCursorStart));
                        printf("\nNEXT CURSOR: %s\n\n\n\n\n", nextCursor.c_str());
                    }
                }
            }
            else {
                twitterObj.getLastCurlError(replyMsg);
                printf( "\ntwitterClient:: twitCurl::followersIdsGet error:\n%s\n", replyMsg.c_str() );
                break;
            }
            searchMutex.unlock();
            // End critical section.

            // Extract follower IDs from web response.
            // Add extracted follower IDs to queue. Only add if not already in crawledIds? Consider efficiency between this and current method.
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
    }
    return;
}


// Takes a set of JSON data from the twitter response and parses out the pfp url for download
std::string parse_pfp_url(std::string xml_data) {
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
            ids.push_back(std::string(match.str()));
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

