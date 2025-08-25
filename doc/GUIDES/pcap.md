SYN, NULL, FIN, XMAS, and ACK are all the TCP

UDP is it's own protocol

SYN scan: Tries to detect open ports
NULL / FIN / XMAS: Try to evade detection or bypass firewall
ACK scan: Detect firewall rules instead of open ports








--------------------
[outdated]
In the Ubuntu VM:
    You can use the command sudo ./pcap_test enp0s3 10.0.2.15 10.0.2.2 22

In Nixos:
    sudo ./pcap_test eth0 10.0.2.15 ??? ?? (try 1.1.1.1 443)


--------------------
[new]
sudo ./pcap_test <find the address of scanme.nmap.org> 80

sudo ./pcap_test 8.8.8.8 443                //roughly equivalent to: sudo nmap -sS -p 443 8.8.8.8

--------------------

# tcpdump
sudo tcpdump -i eth0 host 8.8.8.8           //listens to everything related to that address on that interface. Useful to compare the packets sent and received by nmap and our program

# non immediate mode
Pros of non-immediate mode:
    Better performance: Fewer context switches and syscalls.
    Good for high-throughput scenarios where latency isn’t critical.
Cons:
    Higher latency: You might not get packets for up to the timeout delay.
    Poor responsiveness: If you're building a real-time or interactive tool, this can be a problem.