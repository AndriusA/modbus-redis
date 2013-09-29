Scratch pad for storing data read from a Morningstar Sunsaver MPPT and storing it in Redis.

Starting point for what will be a system to monitor and view solar/charging data.

This runs on a Raspberry Pi. Requires [hiredis] [hired] and [libmodbus] [libmod].

Basis of the modbus from [this blog post] [tom-blog].

[tom-blog]: http://westyd1982.wordpress.com/2010/03/26/linux-and-mac-os-x-software-to-read-data-from-the-sunsaver-mppt-using-modbus/

[hired]: https://github.com/redis/hiredis

[libmod]: https://github.com/stephane/libmodbus