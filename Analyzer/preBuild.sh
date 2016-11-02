#!/bin/bash
dir=`dirname $0`
priorPWD=$PWD
cd $dir

echo $dir >~/Desktop/test.log
#Clear Invalid Data.
rm *.jsonpart
#rm *.json

cd $priorPWD
