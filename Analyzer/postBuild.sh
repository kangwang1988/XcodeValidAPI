#!/bin/bash
dir=`dirname $0`
priorPWD=$PWD
cd $dir

$dir/XcodeValidAPIAnalyzer $dir 8.0 10.0

#Clear Invalid Data.
rm *.jsonpart

cd $priorPWD
