# Objectives

The goal of this project is to make you recode a part of nmap and therefore discover a

new very powerful library.

You will have to use the threads in order to reduce drastically the time spent to scan the

chosen ports.

```
> man nmap
```
This project implies to use mostly the PCAP library (-lpcap) and

THREAD (-lpthread)

# General Instructions

- This project will be corrected by humans only. You’re allowed to organise and name

your files as you see fit, but you must follow the following rules

- You must use C and submit a Makefile
- Your Makefile must compile the project and must contain the usual rules. It must

recompile and re-link the program only if necessary.

- You have to handle errors carefully. In no way can your program quit in an unex-

pected manner (Segmentation fault, bus error, double free, etc).

- Within the mandatory part, you are only allowed to use the entire standard C

library, as well as the entire pcap and pthread libraries.

- You can use other libraries as part of bonuses, but you’ll have to jutify it.


# Mandatory Part

Usage :

> ft_nmap [--help] [--ports [NUMBER/RANGED]] --ip IP_ADDRESS [--speedup [NUMBER]] [--scan [TYPE]]

or

> ft_nmap [--help] [--ports [NUMBER/RANGED]] --file FILE [--speedup [NUMBER]] [--scan [TYPE]]

- The executable must be named ft_nmap.
- A help menu must be available.
- You must only manage a simple IPv4 (address/hostname) as parameter for your

scans.

- You must manage FQDN however you don’t have to make the DNS resolution.
- It must be possible to choose the number of threads (default:0 max:250) to make

the scan faster

- It must be possible to read a list of IPv4 addresses and hostname from a file (for-

matting is free).

- Your program must be able to run the following scans:

◦ SYN, NULL, ACK, FIN, XMAS, UDP

If the scan type is not specified then all scan types must be used.

- We must be able to run each type of scan individually, and several scans simulta-

neously.

- The ports to be scanned can be read as a range or individually. In the case no port

is specified the scan must run with the range 1-1024.

- The number of ports scanned cannot exceed 1024.
- The resolution of service types will be requested (not the version but only the

TYPE).


ft_nmap

- The result of a scan should be as clean and clear as possible. The time frame should

be easy to read.

The format used for both the arguments and the IP list file is free.

For the smarty pants (or not)... Obviously you are NOT allowed to

call a real nmap.

- Here is an example of the help screen:

./ft_nmap --help
Help Screen
ft_nmap [OPTIONS]
--help Print this help screen
--ports ports to scan (eg: 1-10 or 1,2,3 or 1,5-15)
--ip ip addresses to scan in dot format
--file File name containing IP addresses to scan,
--speedup [250 max] number of parallel threads to use
--scan SYN/NULL/FIN/XMAS/ACK/UDP

- The following is an example of a possible result

> ./ft_nmap --ip x.x.x.x --speedup 70 --port 70-90 --scan SYN
Scan Configurations
Target Ip-Address : x.x.x.x
No of Ports to scan : 20
Scans to be performed : SYN
No of threads : 70
Scanning..
........
Scan took 8.32132 secs
IP address: x.x.x.x
Open ports:
Port Service Name (if applicable) Results Conclusion
----------------------------------------------------------------------------------------
80 http SYN(Open) Open

Closed/Filtered/Unfiltered ports:
Port Service Name (if applicable) Results Conclusion
----------------------------------------------------------------------------------------
90 Unassigned SYN(Filtered) Filtered
89 Unassigned SYN(Filtered) Filtered
88 kerberos SYN(Filtered) Filtered
87 link SYN(Filtered) Filtered
86 Unassigned SYN(Filtered) Filtered
85 Unassigned SYN(Filtered) Filtered
84 Unassigned SYN(Filtered) Filtered
83 Unassigned SYN(Filtered) Filtered
82 Unassigned SYN(Filtered) Filtered
81 Unassigned SYN(Filtered) Filtered
79 finger SYN(Filtered) Filtered
78 Unassigned SYN(Filtered) Filtered


ft_nmap

77 rje SYN(Filtered) Filtered
76 Unassigned SYN(Filtered) Filtered
75 Unassigned SYN(Filtered) Filtered
74 Unassigned SYN(Filtered) Filtered
73 Unassigned SYN(Filtered) Filtered
72 Unassigned SYN(Filtered) Filtered
71 Unassigned SYN(Filtered) Filtered
70 gopher SYN(Filtered) Filtered

- The following is an other example of a possible result:

>./ft_nmap --ip x.x.x.x --speedup 200 --port 75-
Scan Configurations
Target Ip-Address : x.x.x.x
No of Ports to scan : 10
Scans to be performed : SYN NULL FIN XMAS ACK UDP
No of threads : 200
Scanning..
................
Scan took 16.21338 secs
IP address: x.x.x.x
Open ports:
Port Service Name (if applicable) Results Conclusion
---------------------------------------------------------------------------------------------------
80 http SYN(Open) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Open

Closed/Filtered/Unfiltered ports:
Port Service Name (if applicable) Results Conclusion
--------------------------------------------------------------------------------------------------
85 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
84 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed)ACK(Unfiltered)
UDP(Open|Filtered) Closed
83 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
82 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Open|Filtered) ACK(Unfiltered)
UDP(Open|Filtered) Closed
81 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
79 finger SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
78 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
77 rje SYN(Filtered) NULL(Open|Filtered)
FIN(Closed) XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
76 Unassigned SYN(Filtered) NULL(Open|Filtered)
FIN(Closed XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed
75 Unassigned SYN(Filtered) NULL(Closed) FIN(Closed)
XMAS(Closed) ACK(Unfiltered)
UDP(Open|Filtered) Closed

# Submission and peer-evaluation

Turn in your assignment in yourGitrepository as usual. Only the work inside your repos-

itory will be evaluated during the defense. Don’t hesitate to double check the names of

your folders and files to ensure they are correct.

- You have to be in a VM with a Linux kernel > 3.14. Note that grading was designed

on a Debian 7.0 stable.
