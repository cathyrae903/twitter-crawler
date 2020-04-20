# Makefile 
ASANFLAGS = -g -fsanitize=address -fno-omit-frame-pointer
FLAGS = -O2
CC=g++

all : twitterCrawler

libcurl : 
		cd ./libtwitcurl && make install 


twitterCrawler : libcurl twitterCrawler.cpp
	$(CC) twitterCrawler.cpp ${FLAGS} -I./include/ -L /usr/local/lib/ -ltwitcurl -lpthread -o crawler

debug: FLAGS=${ASANFLAGS}
debug: twitterCrawler

clean : 
		rm crawler
		rm -dr images
		cd ./libtwitcurl && sudo make clean
