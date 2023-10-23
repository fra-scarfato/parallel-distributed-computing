#!/bin/bash

echo "***********************"
echo "*                     *"
echo "* SUM NUMBERS PROGRAM *"
echo "*                     *"
echo "***********************"
echo ""
read -p "Enter the number of elements you want to sum: " number

#Se non è un numero esce
if ! [[ $number =~ ^[0-9]+$ ]] ; then
   echo "*** ERROR *** Number value uncorrect. Insert an integer number"; 
   exit -1
else 
    #Se il numero <= 20 allora chiede in input i numeri da mettere
    if (( number <= 20)) ; then
        echo "Insert the elements to sum"
        for ((i=0; i<$number; i++)); do
            read -p "Insert the element $i: " array[i]
        done
        #L'array viene rediretto su un file che poi verrà letto dal programma
        echo "${array[@]}" > input.txt
    fi
fi

PS3="Select your strategy (1, 2, 3): "
strategies=("First strategy" "Second strategy" "Third strategy")

select strategy in "${strategies[@]}" Quit
do
    case $REPLY in
        1)
            echo "$strategy selected."
            break;;
        2)
            echo "$strategy selected."
            break;;
        3)
            echo "$strategy selected."
            break;;
        4)
            echo "Goodbye."
            exit -1;;
        *)
            echo "*** ERROR *** Unknown choice";;
    esac
done

strategy=$REPLY

# if [[ "$choice" =~ ^([yY][eE][sS]|[yY])$ ]]
# then
#     #Se la debug mode è attivata aggiungo l'opzione -d
#     $HOME/utility/openmpi/bin/mpiexec -np 4 sum-numbers -n $number -s $strategy -d
# else
    #se numero<=20 viene passato il nome del file su cui ci sono i numeri
    if (( number <= 20 )) ; then
        qsub -v PROJECT="sum-numbers",NP=8,FILE="input.txt",nELEM=$number,S=$strategy sum-numbers.pbs
    #altrimenti gestirà il programma la generazione dell'input
    else
        qsub -v PROJECT="sum-numbers",NP=8,nELEM=$number,S=$strategy sum-numbers.pbs
    fi
#fi





