# Libraries
<stdint.h> (part of the C standard library)
    -uint8_t, ...
<pcap.h>
    -Packet CAPture
<pthread.h>
    -pthread_create(), pthread_join()
    -Links with -pthread on Linux

# Options
--help
    Displays the usage
--ports [NUMBER/RANGE]
    We can specify any range
    We can specify a number, {?in this case the range is 1-X?}
    By default 1-1024
    The number of ports scanned cannot exceed 1024
--ip [ADDRESS] / --file FILE
    One or the other has to be given
    The FILE can be formatted as we wish and contain addresses/hostnames
    We only have to handle ipv4
--speedrup [NUMBER]
    Specify the number of threads used (between 0 and 250) {?The default thread counts, so really we can go up to 251?}
    By default 0
--scan [TYPE]
    By default, all of them should be used
    You can specify one or multiples of them

# Lexicon
FQDN = Fully qualified domain name, like https://www.google.com

# Parsing
    if argv[x] == "--help" -> stop the program and print the help
    if argv[x] != "--ip"/"--file" -> stop the program and print the help
    Then
    --ports = u_int16_t[1024]. Stores each port that needs to be scanned
    --ip = string? --file = malloc for a string[]
    --speedrup = u_int8_t
    --scan = bool[6]

# Threads management
T = Number of threads (including default)
S = Index of the scan type
N = Number of IP Addresses to scan
A = Index of the IP Addresse to scan

((N * S) + A) % T = thread doing the scanning

Do we have to gather results and THEN print them or do we print them as they come? Probably the former, otherwise the output would be disorganized, but now we need to plan a lot of space to store all the results

# Scans
SYN, NULL, ACK, FIN, XMAS, UDP