SYN, NULL, FIN, XMAS, and ACK are all the TCP

UDP is it's own protocol

SYN scan: Tries to detect open ports
NULL / FIN / XMAS: Try to evade detection or bypass firewall
ACK scan: Detect firewall rules instead of open ports








--------------------
In the Ubuntu VM:
    You can use the command sudo ./pcap_test enp0s3 10.0.2.15 10.0.2.2 22

In Nixos:
    sudo ./pcap_test eth0 10.0.2.15 ??? ?? (try 1.1.1.1 443)
