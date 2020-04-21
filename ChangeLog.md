# MID Changelog

## MID Releases

# [v1.0](https://github.com/m0hithreddy/MID/tree/v1.0) (2020-04-04)

* ###### Mohith Reddy < m.mohithreddy99@gmail.com >
  
  * Support for HTTP and HTTPS downloads.
  
  * Support for Chunked Transfer Encoding.

  * Provided support for configuration file (/usr/local/etc/MID.conf).
  
  * Autoconf build system.
  
# [v1.1](https://github.com/m0hithreddy/MID/tree/v1.1) (2020-04-08)

* ###### Mohith Reddy < m.mohithreddy99@gmail.com >
  
  * Made SSL support optional.
  
  * Support for Gzipped encodings.

  * Support for handling content-encodings.
  
  * Few more autoconf file checks and bug fixes.

# [v1.2](https://github.com/m0hithreddy/MID/tree/v1.2) (2020-04-16)

* ###### Mohith Reddy < m.mohithreddy99@gmail.com >
  
  * Saving the MID state using MS (MID State) entries.
  
  * Manupulation of MS entries (add, delete, validate).

  * Support for Download Resume (v1.0).
  
  * Made JUST_RECEIVE obsolete in HTTP API.
  
  * Performance updates and bug fixes.

# [v1.3](https://github.com/m0hithreddy/MID/tree/v1.3) (2020-04-18) (Critical Update)

* ###### Mohith Reddy < m.mohithreddy99@gmail.com >
  
  * Bug Fix: Errors reported by MID_unit were been overlooked by main() because of scheduler decesions, now fixed.
  
  * Bug Fix: Scheduler behaving wrongly at the low download speeds, fixed it now.
  
  * Performance Update: Due to the scheduling delays higher bandwidth interfaces were being idle after they are done with their chunk, now fixed it signaling main().

  * When --interfaces option is specified, use the interfaces in the order requested by the user.
  
  * get_net_if_info(char\*\*,long\*,char\*\*,long\*) --> get_network_interfaces()
 
# [v1.4](https://github.com/m0hithreddy/MID/tree/v1.4) (2020-04-21) (Critical Update)

* ###### Mohith Reddy < m.mohithreddy99@gmail.com >
  
  * Bug Fix: In some cases, max parallel connections crossing --max-downloads, fixed it now.
  
  * Bug Fix: MID hangs when resumed a download which has not left over range with range.end=content_length-1, fixed it now.
  
  * Bug Fix: When err_flag is set (not the fatal_error), some times it is not handled due to scheduler decesions, fixed it now.
  
  * MID_unit writes are lock protected.

  * struct unit_info.total_size is removed, unit_quit() is introduced, refactored MID_unit.c.
  