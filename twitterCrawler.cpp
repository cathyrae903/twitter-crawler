#include "twitterCrawler.h"

int main( int argc, char* argv[] )
{
    // Get credentials from file.
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
    std::string replyMsg;
    char tmpBuf[1024];

    // Set OAuth related params.
    std::ifstream consumerKeyIn;
    std::ifstream consumerKeySecretIn;

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

    // Step 1: Retrieve OAuth access token from files
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
        printf("Found OAuth Access Token\n");
        //printf( "\nUsing:\nKey: %s\nSecret: %s\n\n", myOAuthAccessTokenKey.c_str(), myOAuthAccessTokenSecret.c_str() );

        twitterObj.getOAuth().setOAuthTokenKey( myOAuthAccessTokenKey );
        twitterObj.getOAuth().setOAuthTokenSecret( myOAuthAccessTokenSecret );
    }
    else
    {
        printf("OAuth failed.\n");
        return 511;
    }
    /* OAuth flow ends */

    // Account credentials verification
    if( twitterObj.accountVerifyCredGet() )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf("\nAccount Credential Verification Successful.\n");
        //printf("\ntwitterClient:: twitCurl::accountVerifyCredGet web response:\n%s\n", replyMsg.c_str());
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf("\nAccount Credential Verification Unsuccessful!\n");
        printf("\ntwitterClient:: twitCurl::accountVerifyCredGet error:\n%s\n", replyMsg.c_str());
    }

    std::queue<int> followers;      // Queue for followers that need to be crawled.
    std::vector<int> crawledIds;    // IDs of users that have been crawled/searched?

    std::string nextCursor("");

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
        }

        // Remove this user from the queue.
        followers.pop();
    } while(!nextCursor.empty() && nextCursor.compare("0") && !followers.empty() && depth < MAX_DEPTH);

    
    /**************************************************************************
    *                                Downloader
    **************************************************************************/
    // When code is broken up for multithreading, there will need to be a separate data structure
    // because the searcher's crawledIds data structure is forever changing (additions and sorting).
    std::vector<int>::iterator ptr;
    for(ptr=crawledIds.begin(); ptr < crawledIds.end(); ptr++)
    {
        // Get user information.
        nextCursor = "";
        if( twitterObj.userGet(std::to_string(*ptr), true))
        {
            twitterObj.getLastWebResponse(replyMsg);
            printf( "\ntwitterClient:: twitCurl::userGet web response:\n%s\n", replyMsg.c_str() );

            // Extract profile_image_url_https from replyMsg and download it.
            //system("wget -nd -nH -O ./images/<user_screen_name>.jpg <https_url>");
        }
        else
        {
            twitterObj.getLastCurlError(replyMsg);
            printf( "\ntwitterClient:: twitCurl::userGet error:\n%s\n", replyMsg.c_str() );
        }
    }

    return 0;
}
