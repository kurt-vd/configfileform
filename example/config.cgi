#!/bin/sh

echo "Content-Type: text/html"
echo
cat <<EOF
<html>
<head>
	<title>configfileform test page</title>
</head>
<body>
<h1>config files</h1>
EOF

# process cgi params
if [ "$REQUEST_METHOD" = "POST" ]; then
	echo "<h3>Processing</h3>"

	echo "<pre>"
	CGIREQ=`cat`
	CONFFILE=`../configfileform -v -p conffile -r "$CGIREQ" 2>&1`
	echo "processing $CONFFILE"
	if [ -w "$CONFFILE" ]; then
		../configfileform -v -r "$CGIREQ" "$CONFFILE" -o "$CONFFILE-$$" 2>&1 && mv "$CONFFILE-$$" "$CONFFILE"
		rm -f "$CONFFILE-$$" 2> /dev/null
	else
		echo "file '$CONFFILE' not found"
	fi
	echo "</pre>"
	echo "<hr />"
fi
for FILE in *.conf; do
	if [ ! -w "$FILE" ]; then
		continue;
	fi
	echo "<h3>${FILE%%.conf}</h3>"
	echo "<form method='POST'>"
	../configfileform -v "$FILE"
	echo "<input type='hidden' value='$FILE' name='conffile'>"
	echo "<input type='submit' value='save'>"
	echo "</form>"
done
cat <<EOF
</body>
</html>
EOF
