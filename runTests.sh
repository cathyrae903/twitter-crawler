#!/bin/bash

maxNumSearcher=2
maxNumDownloader=2

touch crawler_results.txt
echo `date` >> crawler_results.txt

for (( s=1; s<=$maxNumSearcher; s++ )); do
	for (( d=1; d<=$maxNumDownloader; d++ )); do
		echo "Running with $s searchers and $d downloaders..."
		`./crawler $s $d > crawler_output_${maxNumSearcher}_${maxNumDownloader}.txt 2>&1`
		echo "Waiting for rate limit to reset..."
		sleep 16m
	done
done
