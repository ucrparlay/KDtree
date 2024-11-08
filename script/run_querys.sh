#!/bin/bash
set -o xtrace

# Solvers=("test" "cgal")
Solvers=("cgal")
Node=(1000000000)
# Tree=(0 1)
Dim=(3)
declare -A datas
datas["/data3/zmen002/kdtree/ss_varden/"]="../benchmark/ss_varden/"
datas["/data3/zmen002/kdtree/uniform/"]="../benchmark/uniform/"

tag=0
k=100
insNum=2
queryType=131072
type="ood_querys"
resFile=""

for solver in "${Solvers[@]}"; do
    exe="../build/${solver}"

    if [[ ${solver} == "test" ]]; then
        resFile="res_${type}.out"
    elif [[ ${solver} == "cgal" ]]; then
        resFile="cgal_${type}.out"
    fi

    for dim in "${Dim[@]}"; do
        for dataPath in "${!datas[@]}"; do
            for node in "${Node[@]}"; do
                files_path="${dataPath}${node}_${dim}"
                log_path="${datas[${dataPath}]}${node}_${dim}"
                mkdir -p "${log_path}"
                dest="${log_path}/${resFile}"
                : >"${dest}"
                echo ">>>${dest}"

                for ((i = 1; i <= insNum; i++)); do
                    numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -q ${queryType} -i 1 -s 0 -r 3 >>${dest}
                done

            done
        done
    done
done

current_date_time="$(date "+%d %H:%M:%S")"
echo $current_date_time
