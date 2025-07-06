# tqftpserv

The tqftpserv software is an implementation of a TFTP (Trivial File Transfer Protocol) server which runs on top of the AF_QIPCRTR (a.k.a QRTR) socket type.

The main purpose of tqftpserv is to serve files from the Linux file system to other processors on the Qualcomm SoCs as requested.

The protocol implemented here is (loosely) based on RFC 1350 including some extensions to the protocol.

In basic terms, the protocol supports RRQ (Read Request) and WRQ (Write Request) messages which read and write files respectively. A request can have some extra options, like `blksize` ior `wsize`. The meaning of those is documented in the `parse_options` function.

For reference, the proprietary implementation of this that is used on practically all Qualcomm-based Android devices is called "tftp_server".

## File paths

There's two different virtual paths prefixes that are supported:
* `/readonly/firmware/image/` for read-only files such as firmware files
* `/readwrite/` for read-write (temporary) files

Requests to those paths are "translated" (see `translate.c`) to paths in the Linux filesystem. In a regular setup readonly requests go to `/lib/firmware/` and readwrite requests go to `/tmp/tqftpserv/`.

Translating those readonly request paths tries to take into account custom Linux firmware paths, custom "firmware-name" paths for remoteprocs, .zstd compressed firmware and more. In case of doubt, please consult the source code.

For example on a QCM6490 Fairphone 5 smartphone the path for `/readonly/firmware/image/modem_pr/so/901_0_0.mbn` will be translated to `/lib/firmware/qcom/qcm6490/fairphone5/modem_pr/so/901_0_0.mbn`, based on the devicetree property `firmware-name = "qcom/qcm6490/fairphone5/modem.mbn";` in the modem/mpss DT node.

The actual paths that are used and requested are dependent on the TFTP clients, which is usually the firmware that is running on the Hexagon-based modem processor on the SoC.

## Example requests

The following are some example requests which are grabbed from the tqftpserv log during modem bootup.

* Write content to a file called "server_check.txt":
```
[TQFTP] WRQ: /readwrite/server_check.txt (octet)
```

* Read a file called "ota_firewall/ruleset" which does not exist:
```
[TQFTP] RRQ: /readwrite/ota_firewall/ruleset (mode=octet rsize=0 seek=0)
tqftpserv: failed to open ota_firewall/ruleset: No such file or directory
[TQFTP] unable to open /readwrite/ota_firewall/ruleset (2), reject
```

* Stat a file with the path "modem_pr/so/901_0_0.mbn", and then read it:
```
[TQFTP] RRQ: /readonly/firmware/image/modem_pr/so/901_0_0.mbn (mode=octet rsize=0 seek=0)
[TQFTP] Remote returned END OF TRANSFER: 9 - End of Transfer
[TQFTP] RRQ: /readonly/firmware/image/modem_pr/so/901_0_0.mbn (mode=octet rsize=52 seek=0)
```

In binary such a request can look like the following:
```
\0\1/readonly/firmware/image/modem_pr/so/901_0_0.mbn\0octet\0blksize\0007680\0timeoutms\0001000\0tsize\0000\0wsize\00010\0
```
