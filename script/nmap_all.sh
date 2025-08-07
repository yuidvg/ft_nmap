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

#80 scanme.nmap.org