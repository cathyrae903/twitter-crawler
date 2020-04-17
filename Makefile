# Makefile 

CC=g++

all : twitterCrawler

libcurl : 
		cd ./libtwitcurl && make install 


twitterCrawler : libcurl twitterCrawler.cpp
	$(CC) twitterCrawler.cpp -I./include/ -L /usr/local/lib/ -ltwitcurl -lpthread -o crawler


clean : 
		rm crawler
		cd ./libtwitcurl && sudo make clean
