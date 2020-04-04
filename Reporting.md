# Reporting

Your bug-report is greatly appreciated! Follow the following steps while reporting any undefined, undesired and unexpected behavaior. Alternatively you can request a feature, which after a thought will be added to TODO list

* [MID](https://github.com/MohithReddy2439/MID/) is expected to be an active project. So before reporting, fetch the latest sources from the master branch and see if the error still persists. 

* You can raise an issue in the github. When reporting upload the command-line arguments and configuration-file. If possible pass ```-VV``` flag and upload the program output (make sure you wont upload any sensitive informations like usernames and passwords). 

* Logging mechanism is still under development, which when completed will simply the reporting mechanism.

# (Might Be) FAQs  

#### 1. In some cases MID is not opening more than one connection, Why?

**A.** This may be because the server is not supporting HTTP range requests, and MID fallbacks to the default download mode (or) HTTP range requests are supported but server is limiting the number of connections (to 1 in this case).

#### 2. MID is not opening the maximum connections that are specified, Why?

**A.** MID wont blindly open the max conections that are passed to it (but it is guranteed that it wont open more than that). Scheduler program takes responsibility in deciding whether to open a new connection or not. The reason behind this is TCP and HTTP overheads. If adding new connection is not boosting the download speed then there is no point in opening any further new connections. If done so, it may decrease the overall performance.

#### 3. In some cases MID performance is poor when used multiple interfaces than single interface, Why?

**A.** The underlying reason for this is also TCP and HTTP overheads, and sometimes program logic and scheduler decisions. Consider your system having two network-interfaces, one being Gigabit ethernet card (eth0) and other being Wireless 802.11 bgn card (wlan0). If the server is able to serve the request at the bandwidth user requested, then clearly eth0 and wlan0 are biased. eth0 is able to capture the speeds of 100 MiBps (consider), whereas wlan0 only gets 10 Mibps (consider). If both network-interfaces are used for download, eth0 downloads its part quickly and works on the part that is assigned to wlan0. This may be decrrease the performance due to TCP and HTTP overheads. Practically speaking the best scenario for this might be to use a single connection on eth0 (even multiple requests on eth0 might hinder the performance).

#### 4. Data Fetched is not summing up to Total Downloaded, Why?

**A.** Two main reasons for this is the Encodings and Syncing Delays. When Trasfer-Encodings are used, the message length will no longer be equal to the actual entity length. More/Less data is fetched based on the encodings used (chunked/gzipped encoding). MID downloading unit (MID unit) is controlled by the parent via thread syncing mechanisms. By the time the flags and variables are updated MID unit might download more data than it is expected to download (genreally in a order of KiB), this difference is generally observed when downloading small files.
 