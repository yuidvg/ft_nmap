The "scantype flag" refers to the option used to specify the type of port scan to be performed. In the context of `ft_nmap`, this flag is **`--scan [TYPE]`**.

The primary goal of port scanning is to detect open ports, identify hosted services, and gather information about distant computers. Nmap, and by extension `ft_nmap` which recodes part of it, classify ports into six states: **open**, **closed**, **filtered**, **unfiltered**, **open|filtered**, or **closed|filtered**. Different scan types leverage various techniques to determine these states and can be used to bypass firewalls or evade intrusion detection systems.

### Scan Types in `ft_nmap`

The `ft_nmap` project mandates the implementation of the following scan types:
*   **SYN**
*   **NULL**
*   **ACK**
*   **FIN**
*   **XMAS**
*   **UDP**

If the scan type is not explicitly specified when running `ft_nmap`, the program must execute **all** of these scan types by default. It must also be possible to run each type individually or several scans simultaneously.

### Detailed Explanation of Scan Types (Drawing from Nmap)

The behavior and advantages of each mandatory scan type implemented in `ft_nmap` are based on techniques used by Nmap:

*   **SYN Scan (TCP SYN scan)**:
    *   In Nmap, this is the default and most popular scan type. It is often referred to as **half-open scanning** because it does not complete the full TCP three-way handshake.
    *   Nmap sends a SYN packet, and if it receives a SYN/ACK, the port is considered **open**. If a RST (reset) packet is received, the port is **closed**. If no response is received after retransmissions, the port is marked as **filtered**.
    *   This scan is relatively unobtrusive and stealthy as it avoids fully establishing connections.

*   **NULL Scan (TCP NULL scan)**:
    *   In Nmap, a Null scan sends TCP packets with **no bits set** in the TCP flag header (TCP flag header is 0).
    *   It exploits a loophole in the TCP RFC (RFC 793): for RFC-compliant systems, a closed port will respond with a RST, while an open port will not respond at all.
    *   If a RST packet is received, the port is considered **closed**. If there is no response, the port is classified as **open|filtered**, meaning it could be open or filtered.
    *   A significant downside is that not all systems (e.g., Microsoft Windows, many Cisco devices) follow this RFC strictly and may send RST responses regardless of port state, leading to all ports being labeled closed.

*   **ACK Scan (TCP ACK scan)**:
    *   In Nmap, this scan sends TCP packets with only the **ACK flag set**.
    *   Its primary use is to **map out firewall rulesets**, rather than determining open ports.
    *   On unfiltered systems, both open and closed ports will return a RST packet, marking them as **unfiltered** (reachable by the ACK packet, but state unknown).
    *   Ports that do not respond or send certain ICMP error messages (e.g., destination unreachable) are labeled **filtered**.

*   **FIN Scan (TCP FIN scan)**:
    *   In Nmap, a FIN scan sets **just the TCP FIN bit**.
    *   Similar to Null scan, it exploits the TCP RFC loophole: a closed port responds with RST, while an open port does not respond.
    *   A RST packet indicates a **closed** port, while no response indicates **open|filtered**.
    *   Like Null and Xmas scans, it can sneak through certain non-stateful firewalls and offers slightly more stealth than a SYN scan, but it may not work reliably against all systems (e.g., Windows).

*   **XMAS Scan (TCP Xmas scan)**:
    *   In Nmap, a Xmas scan sets the **FIN, PSH, and URG flags**, making the packet "light up like a Christmas tree".
    *   It also exploits the same TCP RFC loophole as Null and FIN scans.
    *   A RST packet indicates a **closed** port, and no response means **open|filtered**.
    *   It shares the advantages (potential firewall evasion, stealth) and disadvantages (unreliability against non-RFC compliant systems) with Null and FIN scans.

*   **UDP Scan (UDP scan)**:
    *   In Nmap, this scan works by **sending a UDP packet to every targeted port**.
    *   If an ICMP port unreachable error (type 3, code 3) is returned, the port is **closed**. Other ICMP unreachable errors (e.g., host/network unreachable) mark the port as **filtered**.
    *   If a service responds with a UDP packet, it proves the port is **open**. If no response is received after retransmissions, the port is classified as **open|filtered**.
    *   A challenge with UDP scanning is its speed, as open and filtered ports rarely send responses, leading to timeouts and retransmissions. Many hosts also rate-limit ICMP port unreachable messages, further slowing down the scan.

### Other Nmap Scan Techniques (for context, not specific to `ft_nmap`'s mandatory `--scan` flag)

Nmap offers many other scan techniques beyond those specified for `ft_nmap`:
*   **TCP Connect scan (`-sT`)**: The default TCP scan type when raw packet privileges are not available, it completes the full TCP connection.
*   **TCP Window scan (`-sW`)**: Similar to ACK scan but can differentiate open from closed ports on certain systems by examining the TCP Window field of RST packets.
*   **TCP Maimon scan (`-sM`)**: Similar to Null, FIN, and Xmas scans, but uses FIN/ACK probes.
*   **Custom TCP scan (`--scanflags`)**: Allows users to design their own scan by specifying arbitrary TCP flags.
*   **SCTP INIT scan (`-sY`)**: The SCTP equivalent of a TCP SYN scan, used for SCTP protocols.
*   **SCTP COOKIE ECHO scan (`-sZ`)**: A more advanced SCTP scan that can be more subtle than an INIT scan.
*   **Idle scan (`-sI`)**: A truly blind TCP port scan that exploits predictable IP fragmentation ID sequences on a zombie host to infer port states on the target without sending packets directly from the scanner's real IP.
*   **IP Protocol scan (`-sO`)**: Determines which IP protocols (e.g., TCP, ICMP, IGMP) are supported by target machines, cycling through IP protocol numbers rather than TCP/UDP port numbers.
*   **FTP Bounce scan (`-b`)**: Exploits a feature of the FTP protocol to relay connections through a vulnerable FTP server, making it appear that the scan originates from the FTP server.
