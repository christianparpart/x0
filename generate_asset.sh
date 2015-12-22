#! /bin/bash

INPUTFILE="${1}"
OUTPUTFILE="${2}"
DATASYMBOL="${3}"

DATASIZE=`stat --printf=%s ${INPUTFILE}`

cat >${OUTPUTFILE} <<-EOH
constexpr size_t ${DATASYMBOL}_size = ${DATASIZE};
const unsigned char ${DATASYMBOL}[] = {
EOH

xxd -i <${INPUTFILE} | while read LINE; do
  echo "  ${LINE}"
done >> ${OUTPUTFILE} 

cat >>${OUTPUTFILE} <<-EOT
};
EOT
