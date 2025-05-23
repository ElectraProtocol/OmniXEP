Binaries for Xep version 0.3.21 are available at:
  https://sourceforge.net/projects/xep/files/Xep/xep-0.3.21/

Changes and new features from the 0.3.20 release include:

* Universal Plug and Play support.  Enable automatic opening of a port for incoming connections by running xep or xepd with the - -upnp=1 command line switch or using the Options dialog box.

* Support for full-precision xep amounts.  You can now send, and xep will display, xep amounts smaller than 0.01.  However, sending fewer than 0.01 xeps still requires a 0.01 xep fee (so you can send 1.0001 xeps without a fee, but you will be asked to pay a fee if you try to send 0.0001).

* A new method of finding xep nodes to connect with, via DNS A records. Use the -dnsseed option to enable.

For developers, changes to xep's remote-procedure-call API:

* New rpc command "sendmany" to send xeps to more than one address in a single transaction.

* Several bug fixes, including a serious intermittent bug that would sometimes cause xepd to stop accepting rpc requests. 

* -logtimestamps option, to add a timestamp to each line in debug.log.

* Immature blocks (newly generated, under 120 confirmations) are now shown in listtransactions.
