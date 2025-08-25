if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <port> <ip>"
    exit 1
fi
PORT=$1
IP=$2
# Run all scans, only keeping the result line
nmap -sS -p $PORT $IP | grep "tcp"
nmap -sN -p $PORT $IP | grep "tcp"
nmap -sA -p $PORT $IP | grep "tcp"
nmap -sF -p $PORT $IP | grep "tcp"
nmap -sX -p $PORT $IP | grep "tcp"
nmap -sU -p $PORT $IP | grep "udp"

#sudo sh script/nmap_all.sh 80 scanme.nmap.org
#sudo sh script/nmap_all.sh 81 scanme.nmap.org
#sudo sh script/nmap_all.sh 443 8.8.8.8
#sudo sh script/nmap_all.sh 444 8.8.8.8