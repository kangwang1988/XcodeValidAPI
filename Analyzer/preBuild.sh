#!/bin/bash
dir=`dirname $0`
priorPWD=$PWD
cd $dir

#Clear Invalid Data.
rm *.jsonpart
#rm *.json

cd $priorPWD
