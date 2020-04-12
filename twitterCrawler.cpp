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

    /**************************************************************************
    *                       Twitter Example Functionality
    **************************************************************************/

    /* Get followers' ids */
    std::string nextCursor("");
    std::string searchUser("nextbigwhat");
    do
    {
        if( twitterObj.followersIdsGet( nextCursor, searchUser ) )
        {
            twitterObj.getLastWebResponse( replyMsg );
            printf( "\ntwitterClient:: twitCurl::followersIdsGet for user [%s] web response:\n%s\n",
                    searchUser.c_str(), replyMsg.c_str() );

            // JSON: "next_cursor":1422208797779779359,
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
            twitterObj.getLastCurlError( replyMsg );
            printf( "\ntwitterClient:: twitCurl::followersIdsGet error:\n%s\n", replyMsg.c_str() );
            break;
        }
    } while( !nextCursor.empty() && nextCursor.compare("0") );

    /* Get block list */
    nextCursor = "";
    if( twitterObj.blockListGet( nextCursor, false, false ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::blockListGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::blockListGet error:\n%s\n", replyMsg.c_str() );
    }

    /* Get blocked ids */
    nextCursor = "";
    if( twitterObj.blockIdsGet( nextCursor, true ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::blockIdsGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::blockIdsGet error:\n%s\n", replyMsg.c_str() );
    }

    /* Post a new status message */
    memset( tmpBuf, 0, 1024 );
    printf( "\nEnter a new status message: " );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr = tmpBuf;
    replyMsg = "";
    if( twitterObj.statusUpdate( tmpStr ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusUpdate web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusUpdate error:\n%s\n", replyMsg.c_str() );
    }

    /* Post a new reply */
    memset( tmpBuf, 0, 1024 );
    printf( "\nEnter message id to reply to : " );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr2 = tmpBuf;
    memset( tmpBuf, 0, 1024 );
    printf( "\nEnter a reply message: " );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr = tmpBuf;
    replyMsg = "";
    if( twitterObj.statusUpdate( tmpStr, tmpStr2 ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusUpdate web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusUpdate error:\n%s\n", replyMsg.c_str() );
    }


    /* Search a string */
    printf( "\nEnter string to search: " );
    memset( tmpBuf, 0, 1024 );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr = tmpBuf;
    printf( "\nLimit search results to: " );
    memset( tmpBuf, 0, 1024 );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr2 = tmpBuf;
    replyMsg = "";
    if( twitterObj.search( tmpStr, tmpStr2 ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::search web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::search error:\n%s\n", replyMsg.c_str() );
    }

#ifdef _TWITCURL_TEST_
    /* Get user timeline */
    replyMsg = "";
    printf( "\nGetting user timeline\n" );
    if( twitterObj.timelineUserGet( true, true, 0 ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::timelineUserGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::timelineUserGet error:\n%s\n", replyMsg.c_str() );
    }

    /* Destroy a status message */
    memset( tmpBuf, 0, 1024 );
    printf( "\nEnter status message id to delete: " );
    fgets( tmpBuf, sizeof( tmpBuf ), stdin );
    tmpStr = tmpBuf;
    replyMsg = "";
    if( twitterObj.statusDestroyById( tmpStr ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusDestroyById web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::statusDestroyById error:\n%s\n", replyMsg.c_str() );
    }

    /* Get public timeline */
    replyMsg = "";
    printf( "\nGetting public timeline\n" );
    if( twitterObj.timelinePublicGet() )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::timelinePublicGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::timelinePublicGet error:\n%s\n", replyMsg.c_str() );
    }

    /* Get friend ids */
    replyMsg = "";
    printf( "\nGetting friend ids\n" );
    tmpStr = "techcrunch";
    if( twitterObj.friendsIdsGet( tmpStr, false ) )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::friendsIdsGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::friendsIdsGet error:\n%s\n", replyMsg.c_str() );
    }

    /* Get trends */
    if( twitterObj.trendsDailyGet() )
    {
        twitterObj.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::trendsDailyGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::trendsDailyGet error:\n%s\n", replyMsg.c_str() );
    }
#endif // _TWITCURL_TEST_

    return 0;
}
