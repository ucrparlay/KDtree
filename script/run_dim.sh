#!/bin/bash

Solvers=("test" "cgal")
node=100000000
path="../benchmark/craft_var_dim/"
Dims=(2 3 5 7 9)
SerialTag=(0 1)
T=1800
k=100
wrap=16

resFile=""

for solver in ${Solvers[@]}; do
    for tag in ${SerialTag[@]}; do
        #* decide output file
        if [[ ${solver} == "test" ]]; then
            if [[ ${tag} == 0 ]]; then
                resFile="res_serial.out"
            else
                resFile="res_parallel.out"
            fi
        elif [[ ${solver} == "cgal" ]]; then
            if [[ ${tag} == 0 ]]; then
                resFile="cgal_res_serial.out"
            else
                resFile="cgal_res_parallel.out"
            fi
        fi

        #* dim main body
        for dim in ${Dims[@]}; do
            files_path="${path}${node}_${dim}"
            mkdir -p ${files_path}
            dest="${files_path}/${resFile}"
            : >${dest}
            echo ">>>${dest}"

            for ((i = 1; i <= 2; i++)); do
                nodeVar=$((${node} + ${i}))
                timeout ${T} ../build/${solver} ${nodeVar} ${dim} ${tag} >>${dest}

                retval=$?
                if [ ${retval} -eq 124 ]; then
                    echo -e "${node}_${dim}.in 1200 -1 -1 -1" >>${dest}
                    echo "timeout ${node}_${dim}"
                else
                    echo "finish ${nodeVar}_${dim}"
                fi
            done
        done
    done
done