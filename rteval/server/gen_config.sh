#/bin/sh

APACHECONF="$1"
INSTALLDIR="$2"

echo "Creating Apache config file: apache-rteval.conf"
escinstpath="$(echo ${INSTALLDIR} | sed -e 's/\//\\\\\//g')"
expr=$(echo "s/{_INSTALLDIR_}/${escinstpath}/")
eval "sed -e ${expr} ${APACHECONF}.tpl" > apache-rteval.conf
echo "Copy the apache apache-rteval.conf into your Apache configuration"
echo "directory and restart your web server"
echo

