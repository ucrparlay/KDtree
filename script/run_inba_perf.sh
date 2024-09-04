#!/bin/bash
# {
#     sleep 210m
#     kill $$
# } &
set -o xtrace
Solvers=("test")
Node=(100000000)
Inba=(1 2 5 10 20 30 40 45 48 49 50)
Dim=(3)
declare -A datas
# datas["/ssd0/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

inbaQuery=2
tag=2
k=10
queryType=0 # 001 011 111
echo $queryType

if [[ ${inbaQuery} -eq 0 ]]; then
	resFile="inba_ratio_knn.out"
elif [[ ${inbaQuery} -eq 1 ]]; then
	resFile="inba_ratio_rc.out"
elif [[ ${inbaQuery} -eq 2 ]]; then
	resFile="inba_ratio_perf_simple.out"
fi

for solver in "${Solvers[@]}"; do
	exe="../build/${solver}"
	dest="data/${resFile}"
	: >"${dest}"

	for dim in "${Dim[@]}"; do
		for dataPath in "${!datas[@]}"; do
			for node in "${Node[@]}"; do
				files_path="${dataPath}${node}_${dim}"
				# echo ">>>${dest}"

				# NOTE: run basic first
				# PARLAY_NUM_THREADS=192 INBA_QUERY=${inbaQuery} INBA_BUILD=0 numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} >>"${dest}"

				# NOTE: run others then
				for ratio in "${Inba[@]}"; do
					export PARLAY_NUM_THREADS=192
					export INBALANCE_RATIO=$ratio
					export INBA_QUERY=$inbaQuery
					export INBA_BUILD=1
					command="numactl -i all ${exe} -p "${files_path}/1.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -s 0 -r 3 -i 1"
					output=$($command)
					echo "$ratio $output" >>"$dest"
				done
			done
		done
	done
done

#!/bin/bash