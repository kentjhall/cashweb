#! /bin/sh

RONNV=$(exec ronn -v | grep Ronn | grep 0.7)

if [ ! $? = 0 ] ; then
  echo 'Please install >=ronn-0.7'
  exit 1
fi

echo $0: $(date)

SRCDIR=.
SRCNAMES=$(find ${SRCDIR} -maxdepth 1 -iname '*.ronn' | sed 's/\.ronn//g')

DESTDIR=..
DESTEXT=3.gz

for i in ${SRCNAMES}; do
  echo Processing file: $i
  ronn --pipe -r ${i}.ronn | gzip -c > ${DESTDIR}/${i}.${DESTEXT}
done

echo done.
exit 0

