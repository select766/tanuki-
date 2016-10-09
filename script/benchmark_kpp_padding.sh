#!/bin/sh
OUTPUT_DIRECTORY=`pwd`/benchmark/kpp_padding
RESULT_FILE=${OUTPUT_DIRECTORY}/result.csv
rm -rf ${OUTPUT_DIRECTORY}
mkdir -p ${OUTPUT_DIRECTORY}
for kpp_padding0 in 0 1 2 3 4 5 6 7
do 
  for kpp_padding1 in 0 1 2 3 4 5 6 7
  do 
    echo ${kpp_padding0} ${kpp_padding1}

	pushd ../source
    OUTPUT_FILE=${OUTPUT_DIRECTORY}/${kpp_padding0}-${kpp_padding1}.txt
    make KPP_PADDING0=${kpp_padding0} KPP_PADDING1=${kpp_padding1} -j8 clean pgo
	popd

	pushd ../exe
    ../source/YaneuraOu-by-gcc.exe bench , quit >> ${OUTPUT_FILE}
	popd
	
    echo ${kpp_padding0},${kpp_padding1},`tail -n 1 ${OUTPUT_FILE} | sed 's/[\t ]\+/\t/g' | cut -f3` >> ${RESULT_FILE}
  done
done
