mkdir -p /tmp/myinit
rm result.txt
rm myinit.log
rm -f /tmp/myinit/*
for i in {1..3} 
do 
touch /tmp/myinit/in_${i} 
touch /tmp/myinit/out_${i}
echo "/bin/sleep ${i}s /tmp/myinit/in_${i} /tmp/myinit/out_${i}" >> /tmp/myinit/myinit.config
done
make > /dev/null
./main /tmp/myinit/myinit.config /tmp/myinit/myinit.log


echo "____run TEST 1____"
sleep 3s
echo "Running first three tasks:" > result.txt
echo "$ ps -ef | grep /bin/sleep | head -n 3" >> result.txt
ps -ef | grep /bin/sleep | head -n 3 >> result.txt
pkill -f "/bin/sleep 2s"
sleep 1s
echo "After kill second task:" >> result.txt
echo "$ ps -ef | grep /bin/sleep | head -n 3" >> result.txt
ps -ef | grep /bin/sleep | head -n 3 >> result.txt


echo "____run TEST 2____"
touch /tmp/myinit/in_4
touch /tmp/myinit/out_4
echo "Check update config (should be run only 1 task)" >> result.txt
echo "/bin/sleep 4s /tmp/myinit/in_4 /tmp/myinit/out_4" > /tmp/myinit/myinit.config
killall -SIGHUP main
echo "Sending sighup to daemon:" >> result.txt
sleep 5s
echo "$ ps -ef | grep /bin/sleep | head -n 1" >> result.txt
ps -ef | grep /bin/sleep | head -n 1 >> result.txt

echo "Shutdown myint"
killall -SIGINT main
sleep 4s
cp /tmp/myinit/myinit.log myinit.log
rm -rf /tmp/myinit
rm -f ./main

echo "check result.txt and myinit.log files"
echo "Goodbye!" 