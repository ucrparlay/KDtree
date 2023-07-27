#!/bin/bash
# POSIX
ccore=192

while true; do
    case "$1" in
    --solver)
        shift
        solver=$1
        ;;
    --tag)
        shift
        tag=$1
        ;;
    --dataPath)
        shift
        dataPath=$1
        ;;
    --logPath)
        shift
        logPath=$1
        ;;
    --serial)
        shift
        serial=$1
        ;;
    --node)
        shift
        node=$1
        ;;
    --dim)
        shift
        dim=$1
        ;;
    --insNum)
        shift
        insNum=$1
        ;;
    --core)
        shift
        tmp=$1
        ccore=$((tmp * 2))
        ;;
    --)
        shift
        break
        ;;
    esac
    shift
done

# echo "start, solver: ${solver}, tag ${tag}, data path ${dataPath}, log path ${logPath}, running serail: ${serial}"

T=3600
k=100

#* main body
files_path="${dataPath}${node}_${dim}"
log_path="${logPath}${node}_${dim}"
mkdir -p ${log_path}
dest="${log_path}/${resFile}"
: >${dest}
echo ">>>${dest}"

exe="../build/${solver}"
if [[ ${solver} == "zdtree" ]]; then
    export LD_PRELOAD=/usr/local/lib/libjemalloc.so.2
    exe="/home/zmen002/pbbsbench_x/benchmarks/nearestNeighbors/octTree/neighbors"
fi

for ((i = 1; i <= ${insNum}; i++)); do
    if [[ ${serial} == 1 ]]; then
        PARLAY_NUM_THREADS=1 numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} -r 1 >>${dest}
        continue
    fi
    PARLAY_NUM_THREADS=${ccore} numactl -i all ${exe} -p "${files_path}/${i}.in" -k ${k} -t ${tag} -d ${dim} >>${dest}

    retval=$?
    if [ ${retval} -eq 124 ]; then
        echo -e "${node}_${dim}.in ${T} -1 -1 -1 -1" >>${dest}
        echo "timeout ${node}_${dim}"
    else
        echo "finish ${node}_${dim}"
    fi
done