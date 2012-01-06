#!/bin/sh

TMPSTR=$0
SCRIPT=${TMPSTR##*/}

if [ $# -lt 2 ]; then
	echo "Usage) $SCRIPT module_name api_name"
	exit 1
fi

MODULE=$1
API=$2
TEMPLATE=utc_MODULE_API_func.c.in
TESTCASE=utc_${MODULE}_${API}_func

sed -e	'
	s^@API@^'"$API"'^g
	s^@MODULE@^'"$MODULE"'^g
	' $TEMPLATE > $TESTCASE.c

if [ ! -e "$TESTCASE.c" ]; then
	echo "Failed"
	exit 1
fi
echo "Testcase file is $TESTCASE.c"
echo "Done"
echo "please put \"$TESTCASE\" as Target in Makefile"
echo "please put \"/unit/$TESTCASE\" in tslist"
