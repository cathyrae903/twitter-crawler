#include <regex>
#include <iostream>
#include <stdlib.h>
#include "twitterCrawler.h"


std::string parse_pfp_url(std::string xml_data);
void startDownloader(twitCurl &twitterObj, std::vector<int> &downloadIds);


int main( int argc, char* argv[] )
{
    std::vector<std::thread> downloaderThreads;
    std::queue<int> followers;          // Queue for followers that need to be crawled.
    std::vector<int> crawledIds;        // IDs of users that have been crawled/searched?
    std::vector<int> downloadIds;       // IDs of users whose profile pictures need to bd downloaded.
    std::string nextCursor("");
    std::string twitter_response;       // Stores the xml response

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

    /**************************************************************************
    *                                 Searcher
    **************************************************************************/
    // Prepare a stopping condition of 4 levels deep.
    // This means 
    const int MAX_DEPTH = 1;
    int depth = 0;
    // Start the search with one user's screen-name.
    followers.push(258604828);
    do
    {
        // Only examine this user's followers if this user has not already been crawled.
        if(!std::binary_search(crawledIds.begin(), crawledIds.end(), followers.front()))
        {
            // Increase the depth of the "searcher".
            depth++; 

            // Get this user's followers' IDs.
            if(twitterObj.followersIdsGet(nextCursor, std::to_string(followers.front()), true))
            {
                twitterObj.getLastWebResponse(replyMsg);
                printf( "\ntwitterClient:: twitCurl::followersIdsGet for user [%d] web response:\n%s\n",
                        followers.front(), replyMsg.c_str() );

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

            // Extract follower IDs from web response. 
            // Add extracted follower IDs to queue. Only add if not already in crawledIds? Consider efficiency between this and current method.

            // Mark this user as crawled.
            crawledIds.push_back(followers.front());
            // Sort the vector in ascending order so that search is quicker.
            std::sort(crawledIds.begin(), crawledIds.end());

            downloadIds.push_back(followers.front());
        }

        // Remove this user from the queue.
        followers.pop();
    } while(!nextCursor.empty() && nextCursor.compare("0") && !followers.empty() && depth < MAX_DEPTH);


    downloaderThreads.push_back(std::thread(&startDownloader, std::ref(twitterObj), std::ref(downloadIds)));


    // Wait for execution to finish and join all threads back to main.
    std::vector<std::thread>::iterator itr;
    // Check all threads until none are remaining.
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
void startDownloader(twitCurl &twitterObj, std::vector<int> &downloadIds)
{
    std::string userId;
    std::vector<int>::iterator itr;
    std::string replyMsg;

    for(itr=downloadIds.begin(); itr < downloadIds.end(); itr++)
    {
        // Get user information.
        //nextCursor = "";
        userId = std::to_string(*itr);
        if( twitterObj.userGet(userId, true))
        {
            twitterObj.getLastWebResponse(replyMsg);
            printf( "[+] Attempting to grab pfp...\n");

            // Extract profile_image_url_https from replyMsg and download it.
            system(("curl " + parse_pfp_url(replyMsg) + " --create-dirs -o ./images/" + userId + ".jpg ").c_str());
            printf("[+] Successful pfp grab!\n");
        }
        else
        {
            twitterObj.getLastCurlError(replyMsg);
            printf("\ntwitterClient:: twitCurl::userGet error:\n%s\n", replyMsg.c_str());
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

